#include "uthread.h"
#include "Lock.h"
#include "SpinLock.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

#define UTHREAD_TIME_QUANTUM 10000
#define RANDOM_YIELD_PERCENT 50

// Bank account
static long bank_balance = 0;
static long bank_update_count = 0;

// Shared buffer synchronization
static SpinLock spinlock;
static Lock regular_lock;


void* deposit(void *arg) {
  while (true) {
    // Using lock/spinlock depending on SPINLOCK in uthread.h
  #if SPINLOCK
    spinlock.lock();
  #else
    if (bank_update_count % 1000000 == 0)
        std::cerr << "[" << uthread_self() << "] " << "Deposit locking" << endl;
    regular_lock.lock();
  #endif

    bank_balance += ((rand() * 61) / 7 + bank_update_count % 97);
    if (bank_update_count % 1000000 == 0)
    {
        std::cerr << "Current balance: " << bank_balance << std::endl;
        // this_thread::sleep_for(chrono::milliseconds(500));
        bank_update_count = 0;
    }
    bank_update_count++;

  #if SPINLOCK
    spinlock.unlock();
  #else
    if (bank_update_count  == 1)
        std::cerr << "[" << uthread_self() << "] " << "Deposit unlocking" << endl;
    regular_lock.unlock();
  #endif

    // Randomly give another thread a chance
    if ((rand() % 100) < RANDOM_YIELD_PERCENT) {
      uthread_yield();
    }
  }

  return nullptr;
}

void* withdraw(void *arg) {
  while (true) {
    // Using lock/spinlock depending on SPINLOCK in uthread.h
  #if SPINLOCK
    spinlock.lock();
  #else
    if (bank_update_count % 1000000 == 0)
        std::cerr << "[" << uthread_self() << "] " << "Withdraw locking" << endl;
    regular_lock.lock();
  #endif

    bank_balance -= ((rand() * 61) / 7 + bank_update_count % 97);
    if (bank_update_count % 1000000 == 0)
    {
        std::cerr << "Current balance: " << bank_balance << std::endl;
        // this_thread::sleep_for(chrono::milliseconds(500));
        bank_update_count = 0;
    }
    bank_update_count++;

  #if SPINLOCK
    spinlock.unlock();
  #else
    if (bank_update_count == 1)
        std::cerr << "[" << uthread_self() << "] " << "Withdraw unlocking" << endl;
    regular_lock.unlock();
  #endif

    // Randomly give another thread a chance
    if ((rand() % 100) < RANDOM_YIELD_PERCENT) {
      uthread_yield();
    }
  }

  return nullptr;
}


int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./lock-testcase-bank <num_depositers> <num_withdrawers>" << endl;
    cerr << "Example: ./lock-testcase-bank 20 20" << endl;
    exit(1);
  }

  int depositers = atoi(argv[1]);
  int withdrawers = atoi(argv[2]);

  if ((depositers + withdrawers) > 99) {
    cerr << "Error: <num_depositers> + <num_withdrawers> must be <= 99" << endl;
    exit(1);
  }

  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // Create depositer threads
  int *depositer_threads = new int[depositers];
  for (int i = 0; i < depositers; i++) {
    depositer_threads[i] = uthread_create(deposit, nullptr);
    if (depositer_threads[i] < 0) {
      cerr << "Error: uthread_create deposit" << endl;
    }
  }

  // Create withdrawer threads
  int *withdrawer_threads = new int[withdrawers];
  for (int i = 0; i < withdrawers; i++) {
    withdrawer_threads[i] = uthread_create(withdraw, nullptr);
    if (withdrawer_threads[i] < 0) {
      cerr << "Error: uthread_create withdraw" << endl;
    }
  }

  // NOTE: Depositers and withdrawers run until killed but if we wanted to
  //       join on them do the following

  // Wait for all producers to complete
  for (int i = 0; i < depositers; i++) {
    int result = uthread_join(depositer_threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join deposit" << endl;
    }
  }

  // Wait for all consumers to complete
  for (int i = 0; i < withdrawers; i++) {
    int result = uthread_join(withdrawer_threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join withdraw" << endl;
    }
  }

  delete[] depositer_threads;
  delete[] withdrawer_threads;

  return 0;
}