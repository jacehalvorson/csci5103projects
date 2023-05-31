#include "CondVar.h"
#include "uthread_private.h"

CondVar::CondVar() {
    return;
}

void CondVar::wait(Lock &lock) {
#if DEBUG
    std::cout << "[" << uthread_self( ) << "] Waiting and ";
#endif
    disableInterrupts( );
    if ( mutex_lock == nullptr )
    {
        // Set member variable to remember this lock
        mutex_lock = &lock;
    }

    lock._unlock( );
    
    // Block running thread and place on waiting queue
    lock.addToWaitingQueue( running );

    // Thread should return here after being signaled
#if DEBUG
    std::cout << "[" << running->getId( ) << "] Done waiting and " << std::endl;
#endif

    // Re-acquire lock and re-enable interrupts
    lock.lock( );
    enableInterrupts( );
}

void CondVar::signal()
{
    disableInterrupts( );
    if ( mutex_lock != nullptr )
    {
        mutex_lock->_signal( running );
    }
    enableInterrupts( );
}

void CondVar::broadcast() {
    signal( );
}