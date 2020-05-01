#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>

using namespace std;


struct read_write_lock
{
	pthread_mutex_t common_lock; // Lock for this struct
	int num_readers; // number of readers currently reading
	int num_writers; // used only in writer-pref; number of writers WAITING
};

void InitalizeReadWriteLock(struct read_write_lock * rw);
void ReaderLock(struct read_write_lock * rw);
void ReaderUnlock(struct read_write_lock * rw);
void WriterLock(struct read_write_lock * rw);
void WriterUnlock(struct read_write_lock * rw);