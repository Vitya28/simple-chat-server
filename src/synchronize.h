#ifndef _SYNCHRONIZE_H_
#define _SYNCHRONIZE_H_
#include <pthread.h>

//////////////////////////////////////////////////////////
////////////// SYNCHRONIZATION PRIMATIVES ////////////////
//////////////////////////////////////////////////////////

typedef void (*Operation)( ); // default operation is a function pointer

class Lock
{
  protected:
	typedef pthread_mutex_t Mutex;
	Mutex theLock;

  public:
	Lock( ) { pthread_mutex_init( &theLock, NULL ); }
	~Lock( ) { pthread_mutex_destroy( &theLock ); }
	void lock( ) { pthread_mutex_lock( &theLock ); }
	void unlock( ) { pthread_mutex_unlock( &theLock ); }
};

template <typename GenericOperation>
inline void synchronize( Lock &lock, GenericOperation &operation )
{
	lock.lock( );
		operation( );
	lock.unlock( );
}

//typedef synchronize<Operation> synchronizeOperation;

#endif
