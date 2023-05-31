#include "Lock.h"
#include "uthread_private.h"

Lock::Lock()
{
    // Lock is initialized with ATOMIC_FLAG_INIT
    return;
}

void Lock::lock( )
{
#if DEBUG
    std::cout << "[" << uthread_self( ) << "] Locking" << std::endl;
#endif
    disableInterrupts( );
    while ( atomic_value.test_and_set( ) )
    {
        uthread_yield( );
    }

    // Thread picks up lock when it falls out of while loop
    running->increaseLockCount();
    enableInterrupts( );
}

void Lock::unlock( )
{
#if DEBUG
    std::cout << "[" << uthread_self( ) << "] Unlocking" << std::endl;
#endif
    disableInterrupts( );

    // Release lock
    running->decreaseLockCount();
    atomic_value.clear();

    if ( is_signaled )
    {
    #if DEBUG
        std::cout << "[" << uthread_self( ) << "] Switching back to " << signaling_thread->getId( ) << std::endl;
    #endif
        switchToThread( signaling_thread );
    }

    enableInterrupts( );
}

void Lock::_signal(TCB *tcb)
{
#if DEBUG
    std::queue<TCB *> waiting_queue_copy = waiting_queue;
    std::cout << "[" << uthread_self( ) << "] Signaling - waiting queue: {";

    for ( auto iter = waiting_queue_copy.front( ); !waiting_queue_copy.empty( ); waiting_queue_copy.pop( ) )
    {
        iter = waiting_queue_copy.front( );
        std::cout << iter->getId( ) << ", ";
    }
    std::cout << "}" << std::endl;
#endif

    if ( waiting_queue.empty( ) || is_signaled )
    {
        return;
    }

    // Unblock the thread on the front of the waiting queue
    TCB *queue_head = waiting_queue.front( );
    waiting_queue.pop( );
    
    uthread_resume( queue_head->getId( ) );

    signaling_thread = tcb;
    is_signaled = true;

    _unlock( );
    switchToThread( queue_head );
    lock( );

    is_signaled = false;
    signaling_thread = nullptr;
}

void Lock::_unlock( )
{
#if DEBUG
    std::cout << "[" << uthread_self( ) << "] _Unlocking" << std::endl;
#endif

    // Release lock
    running->decreaseLockCount();
    atomic_value.clear();
}

void Lock::addToWaitingQueue( TCB *tcb )
{
    waiting_queue.push( tcb );
    uthread_suspend( tcb->getId( ) );
}

Priority Lock::highestWaitingPrioritiy( )
{
    std::queue<TCB *> waiting_queue_copy = waiting_queue;
    Priority max = MIN_PRIORITY;

    for ( auto iter = waiting_queue_copy.front( ); !waiting_queue_copy.empty( ); waiting_queue_copy.pop( ) )
    {
        iter = waiting_queue_copy.front( );
        if ( iter->getPriority( ) > max )
        {
            max = iter->getPriority( );
        }
    }
#if DEBUG
    std::cout << "Highest priority = " << max << std::endl;
#endif

    return max;
}
