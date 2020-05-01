#include "alloc.h"
#include <bits/stdc++.h>

using namespace std;

/* Code to allocate page of 4KB size with mmap() call and
* initialization of other necessary data structures.
* return 0 on success and 1 on failure (e.g if mmap() fails)
*/

// Design decisions: we use a SLUB/SLAB type allocator, which tracks free blocks 
// in the granularity of powers of two. We hold a free list for each power of 2,
// starting from 8. The rough steps involved in allocating or deallocating
// are as follows.

// Firstly, there is a in_use_list structure that tracks in_use blocks using metadata structures 
// in the increasing/decreasing order of size, which makes it possible to search if the passed
// pointer to free() is allocated in logarithmic time.
// When a request for an allocation comes in, we first calculate upsize = roundup_to_power_of_2(size)
// We try to find a free block of size upsize in the corresponding free list. If not found, we try to
// find one of 2*upsize and split it into two chunks, and so on. Once a corresponding chunk is found,
// We remove the first size bytes of the block and return it. The remaining chunk is split up into
// chunks which are powers of 2 and reinserted into the corresponding free lists.

// Since this may lead to a lot of fragmentation, we routinely run a defragmentation routine.
// The defragmentation routine checks if any two chunks in bucket n are adjacent to each other. If so,
// the two are removed from the list, coalesced into a chunk of double size and reinserted into the higher
// bucket. This process is recursive and takes place until either no more chunks can be coalesced
// or a fixed number (~ 5) of iterations has completed.

// For free()ing of blocks, first the corresponding entry in the in_use_list (which is actually
// not a list but an RB-tree) is found, it is removed from the list, and the block is reclaimed
// similar to the remainder-block above.

void* base;

struct ChunkEntry{
	// responsible for managing the state of a chunk
	void* begin_address;
	int size;
	bool in_use; // Not really used, but helps in sanity check
};

// Sanity check: size must be a power of 2, unless the block is in use
// buckets for sizes 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 bytes.
// The corresponding indices are 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
// Note that we mmap 32768 bytes but don't let a program alloc more
// than 4096 bytes at once

int size_to_bucket(int sz){
	// Just a helper function
	return log2(sz) - 3;
}

bool sort_helper(ChunkEntry* ce1, ChunkEntry* ce2) {
	// Helps in sorting, for coalescing phase
	return (ce1->begin_address < ce2->begin_address);
	// Dont need to worry about being equal: wont happen
	// if we manage well.
}

inline bool is_power_of_two(int sz){
	if(sz == 1 << int(log2(sz)))
		return true;
	else return false;
}

ChunkEntry* get_chunk_entry(void* beg_addr, int sz, bool use = false){
	ChunkEntry* ce = new ChunkEntry;
	ce->begin_address = beg_addr;
	ce->size = sz;
	ce->in_use = use;
	return ce;
}

vector<ChunkEntry*> get_split_chunks(ChunkEntry* ce) {
	// Destroys ce and instead returns Chunkentries for split chunks
	int sz = ce->size;
	void* begin = ce->begin_address;
	bool inuse = ce->in_use;
	delete ce; // It is necessary that we destroy ce here
	vector<ChunkEntry*> return_vector;
	while(sz){
		int round_down_2 = 1 << (int(log2(sz)));
		return_vector.push_back(get_chunk_entry(begin, round_down_2, inuse));
		sz -= round_down_2;
		begin = (void*)((char*)begin + round_down_2);
	}
	return return_vector;
}

ChunkEntry* get_coalesced_chunk(ChunkEntry* ce1, ChunkEntry* ce2){
	if(!ce1 || !ce2)
		return NULL;
	if(ce1->begin_address > ce2->begin_address)
		swap(ce1, ce2);
	if((char*)(ce1->begin_address) + ce1->size != (char*)(ce2->begin_address))
		return NULL;
	if(ce1->in_use || ce2->in_use)
		return NULL;
	void* new_begin_address = ce1->begin_address;
	int new_size = ce1->size + ce2->size;
	bool new_in_use = false;
	delete ce1, ce2;
	return get_chunk_entry(new_begin_address, new_size, new_in_use);
}

vector<ChunkEntry*> buckets[13];
map<void*, ChunkEntry*> in_use_list; //Not actually a list but an RB tree

bool insert_chunk_into_bucket(ChunkEntry* ce){
	if(!ce)
		return false;
	if(!ce->in_use)
		return false; //cant free already freed memory
	int sz = ce->size;
	if(sz <= 0 || sz > 0x8000)
		return false;
	// Note: even though we allocate only <= 4096-byte chunks, its possible
	// that due to coalescing we obtain the whole 8192 back, so we must accept it
	if(!is_power_of_two(sz)){
		// Split chunk into smaller chunks and try again
		for(ChunkEntry* current_entry : get_split_chunks(ce)){
			insert_chunk_into_bucket(current_entry);
		}
	}
	else{
		// sz is a power of two. Just append to coressponding bucket
		// Don't wory about coalescing for now, otherwise we may have
		// a cascading effect of too many coalesces which affects performance
		int bucket = size_to_bucket(sz);
		ce->in_use = false; // Deallocated
		buckets[bucket].push_back(ce);
	}
}

ChunkEntry* get_sized_subchunk(ChunkEntry* ce, int sz){
	if(ce->in_use)
		return NULL;
	void* begin_address = ce->begin_address;
	int oldsz = ce->size;
	if(sz > oldsz || (sz%8))
		return NULL;
	else if(sz == oldsz)
		return ce;
	ChunkEntry* to_be_returned = get_chunk_entry(begin_address, oldsz, false);
	void* reinsert_begin = (void*)((char*)(begin_address) + sz);
	int reinsert_sz = oldsz - sz;
	ChunkEntry* to_be_reinserted = get_chunk_entry(reinsert_begin, reinsert_sz, true); // make the insertion routine think this was alloced
	insert_chunk_into_bucket(to_be_reinserted);
	delete ce;
	return to_be_returned;
}

void coalesce_where_possible(int num_iter = 1){
	if(num_iter <= 0) return;
	for(int i = 0; i < 12; i++){
		// No point in tring to coalesce 8192 size blocks so i=10 is left out
		std::sort(buckets[i].begin(), buckets[i].end(), sort_helper);
		int current_chunk = 0;
		while(current_chunk + 1 < buckets[i].size()){
			ChunkEntry* ce1 = buckets[i][current_chunk];
			ChunkEntry* ce2 = buckets[i][current_chunk+1];
			if((char*)(ce1->begin_address) + ce1->size == (char*)(ce2->begin_address)){
				buckets[i].erase(buckets[i].begin()+current_chunk+1); // Note that cc+1 must be erased first so as to not invalidate pos-i
				buckets[i].erase(buckets[i].begin()+current_chunk); // If we erased cc first then cc+1 would be in pos cc.
				ChunkEntry* new_chunk_entry = get_coalesced_chunk(ce1, ce2);
				// ce1, ce2 have now been destroyed
				insert_chunk_into_bucket(new_chunk_entry);
				// Now the cc+2 -th elt is at index cc, so no need to increment i
			}
			else{
				current_chunk++;
			}
		}
	}
	if(num_iter)
		coalesce_where_possible(num_iter-1);
}

ChunkEntry* get_free_chunk(int sz){
	if(sz > 4096 || sz <= 0 || (sz%8))
		return NULL; // not allowed
	int get_size;
	if(is_power_of_two(sz))
		get_size = sz;
	else
		get_size = 1 << (1 + (int)log2(sz));
	int bucket = size_to_bucket(sz);
	while(bucket < 13){
		if(buckets[bucket].empty())
			bucket++;
		else break;
	}
	if(bucket == 13){
		// We couldn't find an empty bucket.
		// Try coalescing.
		// If memory is heavily fragmented, it may take some iterations
		// to defragment it. 3 seems fine.
		coalesce_where_possible(3);
		bucket = size_to_bucket(sz);
		while(bucket < 13){
			if(buckets[bucket].empty())
				bucket++;
			else break;
		}
	}
	if(bucket == 13){
		// Still couldnt find free block. Return NULL
		return NULL;
	}
	// Either initially or after coalescing, we have found a free block
	ChunkEntry* found_chunk = buckets[bucket].back();
	buckets[bucket].pop_back();
	return get_sized_subchunk(found_chunk, sz);
}


int init()
{
	// Write your code below
	base = mmap(NULL, 0x8000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FILE | MAP_PRIVATE, -1, 0);
	ChunkEntry* first_chunk = get_chunk_entry(base, 0x8000, true); // true since insert_chunk_.. expects an "in use" chunk
	insert_chunk_into_bucket(first_chunk);
	//cout << base << endl;
}

/* optional cleanup with munmap() call
* return 0 on success and 1 on failure (if munmap() fails)
*/
int cleanup()
{

	// Write your code below
	// Just unmap base
	return munmap(base, 0x8000);
  
}

/* Function to allocate memory of given size
* argument: bufSize - size of the buffer
* return value: on success - returns pointer to starting address of allocated memory
*               on failure (not able to allocate) - returns NULL
*/
char *alloc(int bufSize)
{
	// write your code below
	ChunkEntry* to_be_alloced_chunk = get_free_chunk(bufSize);
	// Now perform two things : insert into in_use_list, and then
	// return the address
	if(!to_be_alloced_chunk)
		return NULL;
	in_use_list[to_be_alloced_chunk->begin_address] = to_be_alloced_chunk;
	return (char*)(to_be_alloced_chunk->begin_address);
}


/* Function to free the memory
* argument: takes the starting address of an allocated buffer
*/
void dealloc(char *memAddr)
{
	// write your code below
	// first verify that this chunk exists in the in_use_list
	void* addr = (void*)memAddr;
	ChunkEntry* ce = in_use_list[addr];
	if(!ce)
		return;
	in_use_list[addr] = NULL; // since now the chunk will be freed
	insert_chunk_into_bucket(ce);
}

