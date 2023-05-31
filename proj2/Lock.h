#ifndef LOCK_H
#define LOCK_H

#include "TCB.h"
#include <queue>
#include <cassert>
#include <atomic>

#define DEBUG 0

// Synchronization lock
class Lock {
public:
  Lock();

  // Attempt to acquire lock. Grab lock if available, otherwise thread is
  // blocked until the lock becomes available
  void lock();

  // Unlock the lock. Wake up a blocked thread if any is waiting to acquire the
  // lock and hand off the lock
  void unlock();

private:
  std::atomic_flag atomic_value = ATOMIC_FLAG_INIT;
  std::queue<TCB *> waiting_queue;
  TCB *signaling_thread;
  bool is_signaled = false;

  // Unlock the lock while interrupts have already been disabled
  // NOTE: Assumes interrupts are disabled
  void _unlock();

  // Let the lock know that it should switch to this thread after the lock has
  // been released (following Hoare semantics)
  // NOTE: Assumes interrupts are disabled
  void _signal(TCB *tcb);

  // Add tcb to the waiting queue
  void addToWaitingQueue(TCB *tcb);

  // Find the highest priority in the waiting queue
  Priority highestWaitingPrioritiy( );

  // Allow condition variable class access to Lock private members
  // NOTE: CondVar should only use _unlock() and _signal() private functions
  //       (should not access private variables directly)
  friend class CondVar;
};

#endif // LOCK_H
