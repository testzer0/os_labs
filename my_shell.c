#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_LINE_SIZE 300

//const char* usr = "/usr/";
int exitcmd = 0;

int handle_call(char** tokens, int start, int end, int ret){
	char* cmd = tokens[start];
	if(!cmd) return 0;
	if(strcmp(cmd, "exit") == 0){
		exitcmd = 1;
		return 0;
	}
	int num_cmds = end - start;

	//for(int i = 1; tokens[i] != NULL; i++)
	//	num_cmds++;

	int ret_val = fork();
	if(ret_val == -1){
		perror("Failed fork()\n");
	}

	else if(ret_val != 0){
		//We are the parent, wait till child completes
		if(ret == 1) return ret_val;
		if(waitpid(ret_val, NULL, 0) == -1){
			perror("Failed while waiting for child.\n");
			exit(-1);
		}
	}
	else{
		char* bin = (char*) malloc(MAX_LINE_SIZE * sizeof(char));
		bin[0] = '\0';
		strcat(bin,"/bin/");
		strcat(bin,cmd);
		char* args[num_cmds + 2];
		args[0] = bin;
		args[num_cmds+1] = NULL;
		for(int i = start; i < end; i++){
			args[i-start+1] = tokens[i];
		}
		//for(int i = 0; i <= num_cmds+1; i++){
		///	printf("%s ", args[i]);
		//}
		///printf("\n");
		if(execvp(args[0], args+1) == -1){
			free(bin);
			return -1;
		}
		free(bin);
		return 0;
	}
}

/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line)
{
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  for(i =0; i < strlen(line); i++){

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){
		tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
		strcpy(tokens[tokenNo++], token);
		tokenIndex = 0; 
      }
    } 
    else {
      token[tokenIndex++] = readChar;
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ;
  return tokens;
}


int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;              
	int i;

	FILE* fp;
	if(argc == 2) {
		fp = fopen(argv[1],"r");
		if(fp < 0) {
			printf("File doesn't exists.");
			return -1;
		}
	}

	while(1) {			
		/* BEGIN: TAKING INPUT */
		if(exitcmd == 1){
			exit(0);
		}
		bzero(line, sizeof(line));
		if(argc == 2) { // batch mode
			if(fgets(line, sizeof(line), fp) == NULL) { // file reading finished
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} else { // interactive mode
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);
   
       //do whatever you want with the commands, here we just print them

		//First check if input is empty
		if(tokens[0] == NULL) continue;
		int separators[64];
		int sep_pos[64];
		int num_segments = 1, total_len = 0;
		separators[0] = 0;
		sep_pos[0] = -1;
		for(int i = 0; tokens[i] != NULL; i++){
			total_len++;
			if(strcmp(tokens[i], (const char*)"&") == 0){
				separators[num_segments] = 1;
				sep_pos[num_segments] = i;
				num_segments++;
			}
			else if(strcmp(tokens[i], (const char*)"&&") == 0){
				separators[num_segments] = 2;
				sep_pos[num_segments] = i;
				num_segments++;
			}
			else if(strcmp(tokens[i], (const char*)"&&&") == 0){
				separators[num_segments] = 3;
				sep_pos[num_segments] = i;
				num_segments++;
			}
		}
		sep_pos[num_segments] = total_len; //to mark end

		int cur = 0;
		pid_t id = waitpid(-1, NULL, WNOHANG);
		if(id == -1){
			
		}
		else if(id){
			printf("Background process exited.\n");
		}
		while(cur < num_segments-1){
			if(separators[cur+1] == 1){
				handle_call(tokens, sep_pos[cur]+1, sep_pos[cur+1], 1);
				cur++;
			}
			else if (separators[cur+1] == 2){
				if(handle_call(tokens, sep_pos[cur]+1, sep_pos[cur+1], 0) == -1)
				 	printf("Incorrect command.");
				cur++;
			}
			else if (separators[cur+1] == 3){
				int st = cur;
				while(cur < num_segments-1 && separators[cur+1] == 3)
					cur++;
				pid_t pids[20];
				int ptr = 0;
				for(int i = st; i <= cur; i++){
					if(exitcmd == 1) break;
					pids[ptr++] = (pid_t) handle_call(tokens, sep_pos[i]+1, sep_pos[i+1], 1);
					if(pids[ptr] == -1){
						printf("Error: wrong command\n");
						break;
					}
					else if(pids[ptr] == 0){
						pids[ptr] = -2;
					}
				}
				while(1){
					pid_t thistime = waitpid(-1, NULL, 0);
					if(thistime == -1){
						perror("Fatal: thread did not exit properly.\n");
						exit(-1);
					}
					int iter = 0, count_unreaped = 0;
					while(iter < ptr){
						if(pids[iter] == thistime)
							pids[iter] = -2;
						if(pids[iter] != -2) count_unreaped++;
						iter++;
					}
					if(count_unreaped == 0) break;
				}
				cur++;
			}
		}
		if(cur < num_segments){
			handle_call(tokens, sep_pos[cur]+1, sep_pos[cur+1], 0);
		}
		// Freeing the allocated memory	
		for(i=0;tokens[i]!=NULL;i++){
			free(tokens[i]);
		}
		free(tokens);

	}
	return 0;
}
