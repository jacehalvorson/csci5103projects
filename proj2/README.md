# Project 2

## Group Members

- Jace Halvorson (halvo561)
- Ryan Johnsen (joh18447)

## 3. Test Cases

### Test Case 1 - Lock/Spinlock (Bank account)

- The file `locks-testcase-bank` has a simple mutex locking scheme where only one "depositer" or "withdrawer" can access the bank at a given time
- This demonstrates the successful implementation of the `Lock` and `Spinlock` classes
- Bank balances are shown in periods of 1 million updates to the bank account

Here's how you would run it using the Makefile:
```
make lock-testcase-bank
./lock-testcase-bank  <num_depositers>  <num_withdrawers>
```

### Test Case 2 - Condition Variable (Bounded Buffer)

- The file `condvar-testcase-buffer` demonstrates the usage of the condition variable class
- It prints information to the console on when threads are acquiring and releasing locks as well as information such as how many items are in the wait queue or are in the buffer itself.

Here's how you would run it using the Makefile:
```
make condvar-testcase-buffer
./condvar-testcase-buffer  <num_producers>  <num_consumers>
```

### Test Case 1 - Priority (Producer/Consumer)

- The file `priority-testcase` takes in the number of producers and consumers as input just like other test cases, but splits them each into 3 priorities evenly.
- This demonstrates the different queues used for different priorities and how priority inversion is dealt with.
- The number of items consumed by each priority level is displayed every time the red threads consume PRINT_FREQUENCY items.

Here's how you would run it using the Makefile:
```
make priority-testcase
./priority-testcase  <num_producers>  <num_consumers>
```

## 4. Performance Evaluation

### 4.1 Lock vs. SpinLock

Test methodology:
- To test the efficiency of the lock and spinlock implementations,
I opted to use a simple form of the bounded buffer problem with locks 
as the only source of synchronization.
- The code for these can be found in `lock-performance.cpp` and `spinlock-performance.cpp`
- I used the linux time utility to calculate the amount of time it took for these programs to consume a specific number of items (20 million) and varied the number of threads

A table of the results can be found in `lock-perf-results.pdf`.

#### Answers to Questions:

- The spinlock seemed to run faster for most of the samples. The timings were fairly variable though, so it's tough to give a strong statement on it. I think the fact that
the uthread library is a uniprocessor library and the fact that the locks were held for a short amount of time helped the spinlock hold up against the regular lock.

- The bigger the critical section, the more CPU cycles that a spinlock will waste, so it is ideal for shorter critical sections. Mutex locks don't see the same performance hits from larger critical sections, but larger critical sections are usually less favorable, since they limit concurrency.

- Spinlocks are more detrimental in a Uniprocessor environment since they waste the only CPU cycles available to the system. They might be better than mutex locks in some cases for multiprocessor systems where it takes longer to wait for CPU resources than just spinning until they can run again.

- Something interesting was that adding more threads wasn't more efficient for many of the samples. I expected the amount of threads to be highly correlated to speeds, but that didn't seem to be the case.