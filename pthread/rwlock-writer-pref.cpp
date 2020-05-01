#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock * rw)
{
  //	Write the code for initializing your read-write lock.
	pthread_mutex_init(&rw->common_lock, NULL);
	rw->num_readers = rw->num_writers = 0;
}

void ReaderLock(struct read_write_lock * rw)
{
  //	Write the code for aquiring read-write lock by the reader.
	while(1){
		pthread_mutex_lock(&rw->common_lock);
		if(rw->num_writers){
			// Oops, there are waiting writers...
			pthread_mutex_unlock(&rw->common_lock);
		}
		else{
			// Can get the lock now
			// Start reading
			rw->num_readers++;
			pthread_mutex_unlock(&rw->common_lock);
			break;
		}
	}
}

void ReaderUnlock(struct read_write_lock * rw)
{
  //	Write the code for releasing read-write lock by the reader.
	pthread_mutex_lock(&rw->common_lock);
	rw->num_readers -= 1;
	pthread_mutex_unlock(&rw->common_lock);
}

void WriterLock(struct read_write_lock * rw)
{
  //	Write the code for aquiring read-write lock by the writer.
	int written = 0;
	while(1){
		// While there are readers reading, gotta wait...
		pthread_mutex_lock(&rw->common_lock);
		if(rw->num_readers){
			// Readers are reading...
			if(!written){
				// Since we are waiting, might as well update to let competing readers/writers know...	
				rw->num_writers += 1;
				written = 1;
			}
			pthread_mutex_unlock(&rw->common_lock);
		}
		else{
			// Readers done reading
			// No other writer holds lock, and no other
			// can get the lock until we release it

			// No longer in contention
			if(written)
				rw->num_writers -= 1;
			break;
		}
	}
}

void WriterUnlock(struct read_write_lock * rw)
{
  //	Write the code for releasing read-write lock by the writer.
	// Just releasing the common lock will do
	pthread_mutex_unlock(&rw->common_lock);
}