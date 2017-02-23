#include "mythread.h"

using namespace std;

pthread_mutex_t MyThread::mutex;

MyThread::MyThread() {}

int MyThread::Create( void *Callback, void *args )
{
	int tret = 0;
	tret = pthread_create( &this->tid, NULL, (void *(*)(void *))Callback, args );

	if ( tret )
	{
		cerr << "Error while creating threads." << endl;
    	return tret;
	}
	else
	{
		//cout << "Thread successfully created." << endl;
    	return 0;
	}
}

int MyThread::Detach()
{
	pthread_detach( this->tid );
}

int MyThread::InitMutex()
{
	if ( pthread_mutex_init( &MyThread::mutex, NULL ) < 0 )
	{
		cerr << "Error while initializing mutex" << endl;
    	return -1;
	}
	else
	{
		//cout << "Mutex initialized." << endl;
    	return 0;
	}
}

int MyThread::LockMutex( const char *identifier )
{
	pthread_mutex_lock( &MyThread::mutex );
}

int MyThread::UnlockMutex( const char *identifier )
{
	pthread_mutex_unlock( &MyThread::mutex );
}










