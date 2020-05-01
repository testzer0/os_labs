#include "simplefs-ops.h"
#include <string.h>
extern struct filehandle_t* file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

// Design decisions: We keep a linked list of created files
// with data on whether the file is currently open, and the inode
// number of the file. For files that are open we keep
// the file handle in the file handle array.

struct file_entry{
	int inode_num;
	char* filename;
	int is_open;
	struct file_entry* next;
};

struct file_entry* get_new_file_entry(char* filename, int inode, struct file_entry* next){
	struct file_entry* fe = (struct file_entry*)malloc(sizeof(struct file_entry));
	fe->inode_num = inode;
	fe->filename = strdup(filename);
	fe->is_open = 0; // Not ppen when created
	fe->next = next;
	return fe;
};

struct file_entry* head = NULL;

int simplefs_create(char *filename){
    /*
	    Create file with name `filename` from disk
	*/
	// Let us first traverse the linked list and ensure 
	// that the file doesn't already exist
	if(strlen(filename) > MAX_NAME_STRLEN)
		return -1;
    struct file_entry* fe = head;
    while(fe){
    	if(strcmp(fe->filename, filename) == 0){
    		return -1;
    	}
    	fe = fe->next;
    }
    // We are here means file entry doesnt always exist
    int alloced_inode = simplefs_allocInode();
    if(alloced_inode == -1)
    	return -1;
    head = get_new_file_entry(filename, alloced_inode, head);
    if(!head){
    	printf("Could not allocate memory for headnode\n");
    	simplefs_freeInode(alloced_inode);
    	return -1;
    }
    struct inode_t* read_inode = (struct inode_t*)malloc(sizeof(struct inode_t));
    simplefs_readInode(alloced_inode, read_inode);

    strcpy(read_inode->name, filename);
    read_inode->file_size = 0; // Nothing has been written to yet
    read_inode->status = INODE_IN_USE;

    simplefs_writeInode(alloced_inode, read_inode);
    free(read_inode);
    return alloced_inode;
}


void simplefs_delete(char *filename){
    /*
	    delete file with name `filename` from disk
	*/
	// Find the node and its predecessor in our linked list
	struct file_entry *cur, *prev;
	prev = NULL, cur = head;
	while(cur){
		if(strcmp(cur->filename, filename) == 0)
			break;
		prev = cur;
		cur = cur->next;
	}
	if(!cur)
		return;
	if(!prev){
		// cur is at head
		head = cur->next;
	}
	else{
		prev->next = cur->next; //unlinked
	}
	free(cur->filename);
	if(cur->is_open)
		simplefs_close(cur->inode_num);

	struct inode_t* read_inode = (struct inode_t*)malloc(sizeof(struct inode_t));
    simplefs_readInode(cur->inode_num, read_inode);
    for(int i = 0; i < MAX_FILE_SIZE; i++){
    	if(read_inode->direct_blocks[i] != -1)
    		simplefs_freeDataBlock(read_inode->direct_blocks[i]);
    }
    free(read_inode);
	simplefs_freeInode(cur->inode_num);
	free(cur);
}

int simplefs_open(char *filename){
    /*
	    open file with name `filename`
	*/
	// First find file_entry for this file
	struct file_entry* fe = head;
	while(fe){
		if(strcmp(fe->filename, filename) == 0)
			break;
		fe = fe->next;
	}
    if(!fe || fe->is_open){
    	return -1; // file doesnt exist or is already open
    }
    int file_index = 0;
    while(file_index < MAX_OPEN_FILES){
    	if(file_handle_array[file_index] == NULL)
    		break;
    	file_index += 1;
    }
    if(file_index == MAX_OPEN_FILES){
    	return -1; // All slots full
    }

    // Fill the index with a filehandle_t
    file_handle_array[file_index] = (struct filehandle_t*) malloc(sizeof(struct filehandle_t));
    if(!file_handle_array[file_index]){
    	printf("Malloc failed.\n");
    	return -1;
    }
    file_handle_array[file_index]->offset = 0;
    file_handle_array[file_index]->inode_number = fe->inode_num;
    fe->is_open = 1;
    return file_index;
}

void simplefs_close(int file_handle){
    /*
	    close file pointed by `file_handle`
	*/
	if(file_handle < 0 || file_handle >= MAX_OPEN_FILES)
		return;
	if(file_handle_array[file_handle] == NULL)
		return;
	int inode = file_handle_array[file_handle]->inode_number;

	free(file_handle_array[file_handle]);
	file_handle_array[file_handle] = NULL; // cleaning up

	struct file_entry* fe = head;
	while(fe){
		if(fe->inode_num == inode){
			fe->is_open = 0; // no longer open; by design only one filentry for a file so break
			break;
		}
		fe = fe->next;
	}

}

int simplefs_read(int file_handle, char *buf, int nbytes){
    /*
	    read `nbytes` of data into `buf` from file pointed by `file_handle` starting at current offset
	*/
	// In a typical system we would have to check that buf doesnt point somewhere malicious
	// i.e. to execv() or to some arguments etc. but we don't worry about that here
	if(file_handle < 0 || file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle] == NULL)
		return -1;
    int inode = file_handle_array[file_handle]->inode_number;
    int offset = file_handle_array[file_handle]->offset;

    struct inode_t* read_inode = (struct inode_t*)malloc(sizeof(struct inode_t));
    simplefs_readInode(inode, read_inode);

    //printf("%d , %d, %d\n", offset, nbytes, read_inode->file_size);
    if(offset + nbytes > MAX_FILE_SIZE*BLOCKSIZE){
    	free(read_inode);
    	return -1;
    }
    int cur_pos = offset, final_pos = offset + nbytes; // 1 beyond last byte
    int bytes_left = nbytes;
    
    char* curr_ptr = buf;
    char* temp = (char*)malloc(BLOCKSIZE);

    while(cur_pos < final_pos){
    	int cur_block = cur_pos / BLOCKSIZE;
    	int in_block_offset = cur_pos % BLOCKSIZE;
    	simplefs_readDataBlock(read_inode->direct_blocks[cur_block], temp);
    	int bytes_to_copy = BLOCKSIZE - in_block_offset;
    	if(bytes_to_copy > bytes_left)
    		bytes_to_copy = bytes_left;
    	memcpy((void*)curr_ptr, (void*)(temp+in_block_offset), bytes_to_copy);
    	cur_pos += bytes_to_copy;
    	curr_ptr += bytes_to_copy;
    	bytes_left -= bytes_to_copy;
    }

    free(temp);
    //file_handle_array[file_handle]->offset += nbytes;
    free(read_inode);

    return 0;
}


int simplefs_write(int file_handle, char *buf, int nbytes){
    /*
	    write `nbytes` of data from `buf` to file pointed by `file_handle` starting at current offset
	*/
    if(file_handle < 0 || file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle] == NULL)
		return -1;
    int inode = file_handle_array[file_handle]->inode_number;
    int offset = file_handle_array[file_handle]->offset;

    struct inode_t* read_inode = (struct inode_t*)malloc(sizeof(struct inode_t));
    simplefs_readInode(inode, read_inode);

    int MAXSIZE = BLOCKSIZE*MAX_FILE_SIZE;
    if(offset + nbytes > MAXSIZE){
    	free(read_inode);
    	return -1;
    }

    int final_block = (offset + nbytes-1)/BLOCKSIZE;
    char* temp2 = (char*)malloc(BLOCKSIZE);
    memset(temp2, 0, BLOCKSIZE);

    for(int i = 0; i <= final_block; i++){
    	if(read_inode->direct_blocks[i] == -1){
    		if((read_inode->direct_blocks[i] = simplefs_allocDataBlock()) == -1){
    			free(temp2);
    			free(read_inode);
    			return -1;
    		}
    		simplefs_writeDataBlock(read_inode->direct_blocks[i], temp2);
    		read_inode->file_size += BLOCKSIZE;
    	}
    }
    int cur_pos = offset, final_pos = offset + nbytes;
    int bytes_left = nbytes;

    char* temp = (char*)malloc(BLOCKSIZE);
    char* curr_ptr = buf;
    //printf("%d and %d\n", cur_pos, final_pos);

    while(cur_pos < final_pos){
    	int cur_block = cur_pos / BLOCKSIZE;
    	int in_block_offset = cur_pos % BLOCKSIZE;
    	int bytes_to_copy = BLOCKSIZE - in_block_offset;
    	simplefs_readDataBlock(read_inode->direct_blocks[cur_block], temp2);
    	if(in_block_offset){
    		memcpy((void*)temp, (void*)temp2, in_block_offset);
    	}
    	if(bytes_to_copy > bytes_left)
    		bytes_to_copy = bytes_left;
    	memcpy((void*)(temp+in_block_offset), curr_ptr, bytes_to_copy);
    	if(in_block_offset + bytes_to_copy < BLOCKSIZE){
    		// Should not garble data beyond what we write
    		memcpy((void*)(temp+in_block_offset+bytes_to_copy), (void*)(temp2+in_block_offset+bytes_to_copy), BLOCKSIZE-(in_block_offset+bytes_to_copy));
    	}
    	simplefs_writeDataBlock(read_inode->direct_blocks[cur_block], temp);

    	cur_pos += bytes_to_copy;
    	curr_ptr += bytes_to_copy;
    	bytes_left -= bytes_to_copy;
    }

    free(temp);
    free(temp2);
    /*file_handle_array[file_handle]->offset += nbytes;
  	if(file_handle_array[file_handle]->offset > read_inode->file_size){
  		read_inode->file_size = file_handle_array[file_handle]->offset;
  	}*/
  	simplefs_writeInode(inode, read_inode);
  	free(read_inode);
  	return 0;
}


int simplefs_seek(int file_handle, int nseek){
    /*
	   increase `file_handle` offset by `nseek`
	*/
	if(file_handle < 0 || file_handle >= MAX_OPEN_FILES)
		return -1;
	if(file_handle_array[file_handle] == NULL)
		return -1;
    int inode = file_handle_array[file_handle]->inode_number;
    int offset = file_handle_array[file_handle]->offset;

    struct inode_t* read_inode = (struct inode_t*)malloc(sizeof(struct inode_t));
    simplefs_readInode(inode, read_inode);
    int new_offset = offset + nseek;
    if(new_offset < 0 || new_offset >= MAX_FILE_SIZE*BLOCKSIZE){
    	free(read_inode);
    	return -1;
    }
    //printf("%d\n", new_offset);
    file_handle_array[file_handle]->offset = new_offset;
    free(read_inode);
    return 0;
}