#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce;
int total_items, max_buf_size, num_workers;
// declare any global data structures, variables, etc that are required
// e.g buffer to store items, pthread variables, etc

typedef struct linked_list_node{
  struct linked_list_node* next;
  int value;
} linked_list_node;

typedef struct linked_list{
  pthread_mutex_t lock; // Every thread performing ops on the ll must hold this lock
  int length_of_linked_list;
  linked_list_node* head;
  int all_ops_finished;
} linked_list;

linked_list* new_linked_list(){
  linked_list* ll = (linked_list*)malloc(sizeof(linked_list));
  if(ll == NULL){
    printf("Unable to allocate linked list structure.\n");
    exit(-1);
  }
  if(pthread_mutex_init(&ll->lock, NULL) != 0){
    printf("Failed to init lock.\n");
    exit(-1);
  }
  ll->length_of_linked_list = 0;
  ll->head = NULL;
  ll->all_ops_finished = 0;
  return ll;
}

linked_list_node* new_node(linked_list_node* next, int value){
  linked_list_node* lln = (linked_list_node*)malloc(sizeof(linked_list_node));
  if(lln == NULL){
    printf("Unable to allocate linked list node.\n");
    exit(-1);
  }
  lln->next = next;
  lln->value = value;
  return lln;
}

linked_list* global_linked_list;

void print_produced(int num) {

  printf("Produced %d\n", num);
}

void print_consumed(int num, int worker) {

  printf("Consumed %d by worker %d\n", num, worker);
  
}


/* produce items and place in buffer (array or linked list)
 * add more code here to add items to the buffer (these items will be consumed
 * by worker threads)
 * use locks and condvars suitably
 */
void *generate_requests_loop(void *data)
{
  int thread_id = *((int *)data);

  while(1)
    {
      if(item_to_produce >= total_items){
        pthread_mutex_lock(&global_linked_list->lock);
        global_linked_list->all_ops_finished = 1;
        pthread_mutex_unlock(&global_linked_list->lock);
        break;
      }
      
      // Lock the linked list and append the entry to the front of the queue
      pthread_mutex_lock(&global_linked_list->lock);
      if(global_linked_list->length_of_linked_list < max_buf_size){
        // There is room for one more entry to be appended
        global_linked_list->head = new_node(global_linked_list->head, item_to_produce);
        global_linked_list->length_of_linked_list += 1;
        print_produced(item_to_produce);
        item_to_produce++;
      }
      pthread_mutex_unlock(&global_linked_list->lock);
    }
  return 0;
}

void* consume_requests_loop(void* data){
  int thread_id = *((int*)data);
  int should_exit = 0;
  while(1){
    if(should_exit)
      break;

    pthread_mutex_lock(&global_linked_list->lock);
    // If linked list is empty then do nothing, but first check if 
    // all entries are done AND list is empty
    if(global_linked_list->length_of_linked_list == 0 && global_linked_list->all_ops_finished){
      should_exit = 1;
    }
    else if(global_linked_list->length_of_linked_list > 0){
      linked_list_node* current_node = global_linked_list->head;
      global_linked_list->head = current_node->next; //unlink
      global_linked_list->length_of_linked_list -= 1;
      print_consumed(current_node->value, thread_id);
      free(current_node);
    }
    pthread_mutex_unlock(&global_linked_list->lock);
  }
}

//write function to be run by worker threads
//ensure that the workers call the function print_consumed when they consume an item

int main(int argc, char *argv[])
{
 
  int master_thread_id = 0;
  pthread_t master_thread;
  item_to_produce = 0;
  pthread_t worker_threads[10000]; // No more than 10000 workers
  
  if (argc < 4) {
    printf("./master-worker #total_items #max_buf_size #num_workers e.g. ./exe 10000 1000 4\n");
    exit(1);
  }
  else {
    num_workers = atoi(argv[3]);
    total_items = atoi(argv[1]);
    max_buf_size = atoi(argv[2]);
  }
  
  // Initlization code for any data structures, variables, etc
  global_linked_list = new_linked_list();


  //create master producer thread
  pthread_create(&master_thread, NULL, generate_requests_loop, (void *)&master_thread_id);

  //create worker consumer threads
  if(num_workers > 3000){
    printf("Too many workers.\n");
    return -1;
  }
  int current_id = 1;
  for(int i = 0; i < num_workers; i++){
    pthread_create(&(worker_threads[i]), NULL, consume_requests_loop, (void*)&current_id);
    current_id += 1;
  }
  
  //wait for all threads to complete
  pthread_join(master_thread, NULL);
  printf("master joined\n");

  for(int i = 0; i < num_workers; i++){
    pthread_join(worker_threads[i], NULL);
    printf("worker %d joined\n", i+1);
  }

  //deallocate and free up any memory you allocated
  free(global_linked_list);
  
  return 0;
}