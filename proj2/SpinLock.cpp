#include "SpinLock.h"
#include "uthread_private.h"

SpinLock::SpinLock()
{
    // Spinlock is initialized with ATOMIC_FLAG_INIT
    return;
}

void SpinLock::lock()
{
    /*
    Lock's flag (atomic_value) is initially set to 0 (ATOMIC_FLAG_INIT). 
    test_and_set returns the status of lock and sets it to 1.

    1. If the lock is available (0), then test_and_set returns 0 and thread 
    falls out of spinlock loop while setting the lock's status to not-available (1). 
    
    2. If the lock is not available (1), then test_and_set continually returns 1 and
    thread spins until thread holding the lock releases it (atomic_value.clear())
    */
    while(atomic_value.test_and_set());
    running->increaseLockCount();
}

void SpinLock::unlock()
{
    // Makes lock available to other threads (set atomic_value to 0)
    atomic_value.clear();
    running->decreaseLockCount();
}