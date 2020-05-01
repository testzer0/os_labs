#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock * rw)
{
  //	Write the code for initializing your read-write lock.
	pthread_mutex_init(&rw->common_lock, NULL);
	rw->num_readers = 0;
}

void ReaderLock(struct read_write_lock * rw)
{
  //	Write the code for aquiring read-write lock by the reader.
	// Wait to acquire overall lock
	pthread_mutex_lock(&rw->common_lock);
	// We are here means no writers were waiting when
	// we got the lock
	rw->num_readers += 1;
	// We can release the lock now since writers will see that not all readers
	// have finished
	pthread_mutex_unlock(&rw->common_lock);
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
	while(1){
		pthread_mutex_lock(&rw->common_lock);
		// Check if all readers have finished
		if(rw->num_readers){
			// Release the lock since not all readers have finished
			pthread_mutex_unlock(&rw->common_lock);
		}
		else{
			// All readers finished. Break
			break;
		}
	}
}

void WriterUnlock(struct read_write_lock * rw)
{
  //	Write the code for releasing read-write lock by the writer.
	pthread_mutex_unlock(&rw->common_lock);
}