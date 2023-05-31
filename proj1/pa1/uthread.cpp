#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>
#include <sys/time.h>
#include <csignal>
#include <cstring>

using namespace std;

// Supporting macros ------------------------------------------------------------------
#define MAIN_TID                           0

#define M_GetTCBPtr( tid )                 ( thread_array[ tid ] )
#define M_IsThreadInState( tid, state )    ( M_GetTCBPtr( tid )->getState( ) == state )
#define M_AddToThreadArray( tcbPtr )       ( thread_array[ tcbPtr->getId( ) ] = tcbPtr )
#define M_RemoveFromThreadArray( tcbPtr )  ( thread_array[ tcbPtr->getId( ) ] = nullptr )

#define MICROSECONDS_PER_SEC               1000000

void sig_handler( int sig );
#if DEBUG
void printQueue( State state );
#endif

// Type definitions -------------------------------------------------------------------
// Finished queue entry type
typedef struct finished_queue_entry {
  int tid;              // TID (TCB has been deallocated because thread is finished)
  void *result;         // Pointer to thread result (output)
} finished_queue_entry_t;

// Join queue entry type
typedef struct join_queue_entry {
  TCB *tcb;             // Pointer to TCB
  int waiting_for_tid;  // TID this thread is waiting on
} join_queue_entry_t;

// Queues
static deque<TCB*> ready_queue;
static deque<TCB*> blocked_queue;
static deque<join_queue_entry> join_queue;
static deque<finished_queue_entry> finished_queue;

// ARRAY giving access to any TCB indexed by TID
static TCB* thread_array[ MAX_THREAD_NUM ];

// Global variables
static int thread_count = 0;
TCB* running_thread = nullptr;
sigset_t oldSignalMask;
struct itimerval timer;
int quantum_us = 0;
int total_quantums = 0;

// Interrupt Management --------------------------------------------------------

// Start a countdown timer to fire an interrupt
static void startInterruptTimer()
{
    struct sigaction act;

#if DEBUG
    cout << "Starting interrupt timer" << endl;
#endif

    // Set up signal handler for timer
    memset( &act, 0, sizeof( act ) );
    act.sa_handler = sig_handler;

    // Allow this signal to arrive in its handler
    act.sa_flags = SA_NODEFER;

    // Make sure SIGVTALRM isn't blocked
    sigemptyset( &act.sa_mask );
    sigaddset( &act.sa_mask, SIGVTALRM );
    if ( sigprocmask( SIG_UNBLOCK, &act.sa_mask, nullptr ) != 0 )
    {
        perror( "sigprocmask: " );
        return;
    }

    // Retrive the current signal mask so it doesn't change after sigaction
    if ( sigprocmask( SIG_UNBLOCK, nullptr, &act.sa_mask ) != 0 )
    {
        perror( "sigprocmask: " );
        return;
    }

    // Set SIGVTALRM signal handler to sig_handler
    if ( sigaction( SIGVTALRM, &act, nullptr ) != 0 )
    {
        perror( "sigaction: " );
        return;
    }

    timer.it_interval.tv_sec = quantum_us / MICROSECONDS_PER_SEC; // us -> s
    timer.it_interval.tv_usec = quantum_us % MICROSECONDS_PER_SEC; // Leftover microseconds
    timer.it_value = timer.it_interval;
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) == -1) {
        cerr << "Failed to set timer" << endl;
    }
}

// Block signals from firing timer interrupt
static void disableInterrupts()
{
    sigset_t fullSignalMask;
#if DEBUG
    cout << "Disabling interrupts" << endl;
#endif
    sigemptyset( &fullSignalMask );
    sigaddset( &fullSignalMask, SIGVTALRM );
    sigprocmask( SIG_BLOCK, &fullSignalMask, &oldSignalMask );
}

// Unblock signals to re-enable timer interrupt
static void enableInterrupts()
{
#if DEBUG
    cout << "Enabling interrupts" << endl;
#endif
    sigprocmask( SIG_SETMASK, &oldSignalMask, nullptr );
}


// Queue Management ------------------------------------------------------------

// Add TCB to the back of the ready queue
void addToReadyQueue(TCB *tcb)
{
    ready_queue.push_back(tcb);

#if DEBUG
    cout << "Thread #" << tcb->getId() << " being added to ";
    printQueue( READY );
#endif
}

// Removes and returns the first TCB on the ready queue
// NOTE: Assumes at least one thread on the ready queue
TCB* popFromReadyQueue()
{
    assert(!ready_queue.empty());

    TCB *ready_queue_head = ready_queue.front();
    ready_queue.pop_front();

#if DEBUG
    cout << "Thread #" << ready_queue_head->getId() << " being popped from ";
    printQueue( READY );
#endif

    return ready_queue_head;
}

void addToBlockedQueue(TCB *tcb)
{
    blocked_queue.push_back( tcb );
#if DEBUG
    cout << "Thread #" << tcb->getId() << " being added to ";
    printQueue( BLOCK );
#endif
}

int removeFromQueue( TCB *tcbPtr )
{
    int tid = tcbPtr->getId( );

#if DEBUG
    cout << "Thread #" << tid << " being removed from ";
    printQueue( tcbPtr->getState( ) );
#endif

    // Take off whatever queue this TCB is in
    switch ( tcbPtr->getState( ) )
    {
    case READY:
        for ( deque<TCB*>::iterator iter = ready_queue.begin( ); iter != ready_queue.end( ); ++iter )
        {
            if ( tid == (*iter)->getId( ) )
            {
                ready_queue.erase( iter );
                return 0;
            }
        }

        // Thread not found
        break;

    case BLOCK:
        for ( deque<TCB*>::iterator iter = blocked_queue.begin( ); iter != blocked_queue.end( ); ++iter )
        {
            if ( tid == (*iter)->getId( ) )
            {
                blocked_queue.erase( iter );
                return 0;
            }
        }

        // Thread not found, return error
        cout << "not found in blocked queue" << endl;
        return -1;

    case RUNNING:
        // If it is running it is not in any queue, return success
        break;
    default:
        // If the state is unknown, return error
        return -1;
    }

    return 0;
}

int addToFinishedQueue( TCB *tcb, void *result )
{
    finished_queue_entry finishedQueueEntry;
#if DEBUG
    cout << "Moving thread #" << tcb->getId( ) << " to "; 
#endif

    // Initialize struct to add to the finished queue
    memset( &finishedQueueEntry, 0, sizeof( finishedQueueEntry ) );
    finishedQueueEntry.tid = tcb->getId( );
    finishedQueueEntry.result = result;

    // Add this entry to the finished queue
    finished_queue.push_back( finishedQueueEntry );
#if DEBUG
    printQueue( FINISH );
#endif
    return 0;
}

void *popFinishedQueueEntry( int tid )
{
#if DEBUG
    cout << "Popping thread #" << tid << " from finished queue......";
#endif

    for ( deque<finished_queue_entry>::iterator iter = finished_queue.begin(); iter != finished_queue.end(); ++iter )
    {
        if ( (*iter).tid == tid )
        {
            void *result = (*iter).result;
        #if DEBUG
            cout << "found, result = " << *( (unsigned long*) result ) << " from ";
            printQueue( FINISH );
        #endif
            finished_queue.erase( iter );
            return result;
        }
    }

    // tid wasn't found in finished queue
#if DEBUG
    cout << "not found in ";
    printQueue( FINISH );
#endif
    return nullptr;
}


// Helper functions ------------------------------------------------------------

// Switch to the next ready thread
static void switchThreads( )
{
    TCB* new_running_thread;
    TCB* old_running_thread;

#if DEBUG
    cout << "Switching threads" << endl;
#endif

    old_running_thread = running_thread;
    new_running_thread = popFromReadyQueue();
    new_running_thread->setState(RUNNING);

    volatile int flag = 0;
    if ( running_thread != nullptr )
    {
        // Previous thread isn't finished, save their context
        running_thread->saveContext();
    }
    if (flag == 1)
    {
        return;
    }
    flag = 1;

#if DEBUG
    cout << "new_running_thread tid = " << new_running_thread->getId() << endl;
#endif

    // Set running thread global variable
    running_thread = new_running_thread;

    // Start timer for next quantum and enable interrupts
    startInterruptTimer( );
    new_running_thread->loadContext( );
}

#if DEBUG
// Takes in the state of the threads inside the queue being printed
// 0 - READY
// 1 - RUNNING (nothing to print)
// 2 - BLOCK
// 3 - FINISH
void printQueue( State queueType )
{
    switch ( queueType )
    {
    case READY:
        cout << "Ready queue: { ";
        if ( !ready_queue.empty( ) )
        {
            cout << (*ready_queue.begin( ))->getId( );
            for ( deque<TCB*>::iterator iter = ready_queue.begin()+1; iter != ready_queue.end(); ++iter )
            {
                cout << ", " << (*iter)->getId( );
            }
        }
        cout << " }";

        break;

    case BLOCK:
        cout << "Blocked queue: { ";
        if ( !blocked_queue.empty( ) )
        {
            cout << (*blocked_queue.begin( ))->getId( );
            for ( deque<TCB*>::iterator iter = blocked_queue.begin()+1; iter != blocked_queue.end(); ++iter )
            {
                cout << ", " << (*iter)->getId( );
            }
        }
        cout << " }";

        break;

    case FINISH:
        cout << "Finished queue: { ";
        if ( !finished_queue.empty( ) )
        {
            cout << (*finished_queue.begin( )).tid;
            for ( deque<finished_queue_entry>::iterator iter = finished_queue.begin()+1; iter != finished_queue.end(); ++iter )
            {
                cout << ", " << (*iter).tid;
            }
        }
        cout << " }";

        break;
    
    default:
        cerr << "Invalid queue type " << queueType;
    }

    cout << endl;
}
#endif

// Library functions -----------------------------------------------------------

// The function comments provide an (incomplete) summary of what each library
// function must do

void sig_handler(int sig)
{
#if DEBUG
    cout << "Inside signal handler" << endl;
#endif

    disableInterrupts( );
    if ( running_thread != nullptr )
    {
        running_thread->setState( READY );
        addToReadyQueue( running_thread );
        running_thread->increaseQuantum( );
        total_quantums++;
    }

    switchThreads( );
    enableInterrupts( );
#if DEBUG
    cout << "Returning from switchThreads" << endl;
#endif
}

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg)
{
    void* retval = start_routine( arg );
    uthread_exit( retval );
}

int uthread_init(int quantum_usecs)
{
    struct sigaction act;

    if ( quantum_usecs < 0 )
    {
        cerr << "init: Invalid quantum" << endl;
        return -1;
    }

    // Set up signal handler for timer
    memset( &act, 0, sizeof( act ) );
    act.sa_handler = sig_handler;

    // Allow this signal to arrive in its handler
    act.sa_flags = SA_NODEFER;

    // Make sure SIGVTALRM isn't blocked
    sigemptyset( &act.sa_mask );
    sigaddset( &act.sa_mask, SIGVTALRM );
    if ( sigprocmask( SIG_UNBLOCK, &act.sa_mask, nullptr ) != 0 )
    {
        perror( "sigprocmask: " );
        return -1;
    }

    // Retrive the current signal mask so it doesn't change after sigaction
    if ( sigprocmask( SIG_UNBLOCK, nullptr, &act.sa_mask ) != 0 )
    {
        perror( "sigprocmask: " );
        return -1;
    }

    // Set SIGVTALRM signal handler to sig_handler
    if ( sigaction( SIGVTALRM, &act, nullptr ) != 0 )
    {
        perror( "sigaction: " );
        return -1;
    }

    // Setting up timer to repeat every quantum amount of time
    quantum_us = quantum_usecs;

    // Create a thread for the caller (main) thread
    TCB* mainThread = new TCB( thread_count, nullptr, nullptr, RUNNING );
    thread_count++;
    M_AddToThreadArray( mainThread );
    running_thread = mainThread;

    return 0;
}

int uthread_create(void* (*start_routine)(void*), void* arg)
{
    if ( thread_count >= MAX_THREAD_NUM )
    {
        cerr << "FAILURE: Cannot exceed maximum thread count" << endl;
        return -1;
    }

    // Create a new thread and add it to the thread array and ready queue
    TCB* new_thread = new TCB(thread_count, start_routine, arg, READY);

    disableInterrupts( );

    // Update global variables
    thread_count++;
    M_AddToThreadArray( new_thread );

    // Add this thread onto the end of the ready queue
    addToReadyQueue( new_thread );

    enableInterrupts( );

    if ( new_thread->getId( ) == 1 )
    {
        startInterruptTimer();
    }

    return new_thread->getId();
}

int uthread_join(int tid, void **retval)
{
    unsigned long *result;

    if ( ( tid > MAX_THREAD_NUM-1 ) ||
         ( tid < 0 ) )
    {
        cerr << "join: Invalid tid" << endl;
        return -1;
    }
#if DEBUG
    cout << "Joining on tid " << tid << ", ";
#endif

    // Check if tid has already terminated
    disableInterrupts( );
    result = (unsigned long *) popFinishedQueueEntry( tid );
    enableInterrupts( );

    if ( result != nullptr )
    {
        // tid is already in finished queue, send result back to joining thread by
        // setting *retval to be the result of thread if retval != nullptr
        if ( retval != nullptr )
        {
            *retval = result;
        }
    #if DEBUG
        else
        {
            cout << "Don't care about return value" << endl;
        }
    #endif

        return 0;
    }

    // Null pointer check after looking in finished queue because deallocated threads
    // removed from thread array can still have finished queue entries
    if ( thread_array[ tid ] == nullptr )
    {
        cerr << "invalid thread" << endl;;
        return -1;
    }

    if ( M_IsThreadInState( tid, READY ) ||
         M_IsThreadInState( tid, BLOCK ) )
    {
        disableInterrupts( ); // re-enabled after switchThreads
        // tid we are joining is still active.
        // Fill out struct to add calling thread to join queue
        join_queue_entry joinQueueEntry;
        memset( &joinQueueEntry, 0, sizeof( joinQueueEntry ) );
        joinQueueEntry.tcb = running_thread;
        joinQueueEntry.waiting_for_tid = tid;

        // Add this entry to join queue
        join_queue.push_back( joinQueueEntry );

    #if DEBUG
        cout << "still running, block thread #" << running_thread->getId( ) << endl;
    #endif

        // Block calling thread until tid terminates
        running_thread->setState( BLOCK );
        switchThreads( );
        enableInterrupts( );
        // When this thread is activated again, restart the join because tid is finished
        return uthread_join( tid, retval );
    }

    // Should never be reached
    return -1;
}

int uthread_yield(void)
{
    if ( running_thread == nullptr )
    {
        cerr << "yield: No running thread" << endl;
        return -1;
    }

#if DEBUG
    cout << "Thread " << running_thread->getId( ) << " is yielding" << endl;
#endif
    // Place this thread on the ready queue with interrupts disabled
    disableInterrupts( );
    running_thread->setState( READY );
    addToReadyQueue( running_thread );
    enableInterrupts( );

    disableInterrupts( );
    switchThreads( );
    enableInterrupts( );
    return 0;
}

void uthread_exit(void *retval)
{
    if ( running_thread == nullptr )
    {
        cerr << "exit: No running thread" << endl;
        return;
    }
    
    int tid = running_thread->getId( );
    TCB *tcbPtr = running_thread;
    TCB *waitingTcbPtr;
#if DEBUG
    cout << "Thread #" << tid << " is exiting" << endl;
#endif

    if ( tid == MAIN_TID )
    {
        // This is the main thread, exit the program
        delete M_GetTCBPtr( MAIN_TID );
        exit( 0 );
    }

    disableInterrupts( ); // re-enabled after switchThreads()

    // Move any threads joining on this thread back to the ready queue
#if DEBUG
    cout << "Looking through join queue for a thread waiting on #" << tid << "......";
#endif
    for ( deque<join_queue_entry>::iterator iter = join_queue.begin( ); iter != join_queue.end( ); iter++ )
    {
        waitingTcbPtr = (*iter).tcb;
        if ( (*iter).waiting_for_tid == tid )
        {
            // Remove from join queue and place on ready queue with state READY
            join_queue.erase( iter );
            waitingTcbPtr->setState( READY );
            addToReadyQueue( waitingTcbPtr );
        #if DEBUG
            cout << "found, tid = " << waitingTcbPtr->getId( ) << " ";
        #endif
            break;
        }
    }

#if DEBUG
    cout << endl;
#endif

    // Remove this thread from whatever queue it is in
    removeFromQueue( tcbPtr );
    // Add this thread to the finished queue
    addToFinishedQueue( tcbPtr, retval );

    // Update global variables
    M_RemoveFromThreadArray( tcbPtr );
    thread_count--;

    running_thread = nullptr;

    // Switch to next thread on the ready queue (re-enables interrupts)
    switchThreads( );
    enableInterrupts( );
}

int uthread_suspend(int tid)
{
    TCB *tcbPtr;
#if DEBUG
    cout << "Suspending thread " << tid << endl;
#endif
    
    if ( ( tid > MAX_THREAD_NUM-1 )         ||
         ( tid < 0 )                        ||
         ( thread_array[ tid ] == nullptr ) || 
         ( M_GetTCBPtr( tid )->getState( ) == BLOCK ) )
    {
        cerr << "Suspend failed" << endl;
        return -1;
    }
    
    // Find TCB pointer associated with tid
    tcbPtr = M_GetTCBPtr( tid );

    disableInterrupts( );

    // Remove from current queue if it's in one
    removeFromQueue( tcbPtr );
    // Add to block queue with state BLOCK
    tcbPtr->setState( BLOCK );
    addToBlockedQueue( tcbPtr );

    enableInterrupts( );

    if ( running_thread->getId( ) == tid )
    {
        // Running thread is suspending itself, switch to a different thread
        disableInterrupts( );
        switchThreads( );
        enableInterrupts( );
    }

    return 0;
}

int uthread_resume(int tid)
{
    TCB *tcbPtr;

#if DEBUG
    cout << "Resuming thread " << tid << endl;
#endif

    if ( ( tid > MAX_THREAD_NUM-1 )         ||
         ( tid < 0 )                        ||
         ( thread_array[ tid ] == nullptr ) || 
         ( M_GetTCBPtr( tid )->getState( ) != BLOCK ) )
    {
        cerr << "Resume failed" << endl;
        exit(0);
        return -1;
    }

    // Find TCB pointer associated with tid
    tcbPtr = M_GetTCBPtr( tid );

    disableInterrupts( );

    // Remove thread from the queue it's currently in (likely blocked queue)
    removeFromQueue( tcbPtr );
    // Add thread to ready queue with state READY
    tcbPtr->setState( READY );
    addToReadyQueue( tcbPtr );

    enableInterrupts( );

    return 0;
}

int uthread_self()
{
    return running_thread->getId( );
}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums( int tid )
{
    if ( thread_array[ tid ] == nullptr )
    {
        return -1;
    }

    return ( M_GetTCBPtr( tid )->getQuantum( ) );
}
