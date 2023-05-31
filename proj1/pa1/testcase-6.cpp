#include "uthread.h"
#include <iostream>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    cout << "thread id: " << my_tid << endl;

    int NumIterations = ( my_tid + 100 ) * 2 ;
    unsigned long counter = 0;
    unsigned long *num_primes = new unsigned long;
    bool hasAFactor;

    for ( int i = 0; i < NumIterations; i++ )
    {
        for ( int i = 0; i < NumIterations; i++ )
        {
            for ( int i = 0; i < NumIterations; i++ )
            {
                counter++;
            }   
        }
    }

    *num_primes += counter;
    return num_primes;
}

int main(int argc, char *argv[]) {
    int quantum_usecs = 1000;
    
    unsigned int thread_count = 99;
    int *threads = new int[thread_count];

    // Init user thread library
    int ret = uthread_init(quantum_usecs);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    long counter = 0;

    // Attempt to create more threads than are allowed (99 plus main thread).
    // Should see one error message
    for (int i = 0; i < thread_count+1; i++) {
        int tid = uthread_create(worker, nullptr);
        if ( tid != -1 )
        {
            threads[i] = tid;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        // Add thread result to global total
        unsigned long *return_value;
        uthread_join(threads[i], (void**)&return_value);
        
        // Deallocate thread result
        cout << "Thread " << threads[i] << " returned " << *return_value << endl;
        counter += *return_value;
        delete return_value;
    }

    delete[] threads;

    cout << "Final result: " << counter << endl;

    return 0;
}
