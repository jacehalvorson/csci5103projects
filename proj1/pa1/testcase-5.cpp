#include "uthread.h"
#include <iostream>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    cout << "thread id: " << my_tid << endl;

    int NumIterations = 1000;
    int prime_number_counter = 0;
    int *num_primes = new int;
    bool hasAFactor;

    for ( int i = 2; i < NumIterations; i++ )
    {
        hasAFactor = false;
        for ( int j = 2; j < i; j++ )
        {
            if ( i % j == 0 )
            {
                hasAFactor = true;
            }
        }
        if (!hasAFactor)
        {
            prime_number_counter++;
            if ( my_tid % 8 == 0 )
            {
                *num_primes = prime_number_counter;
                cout << "Thread #" << my_tid << " yielding control" << endl;
                uthread_yield( );
            }
        }
    }

    *num_primes = prime_number_counter;
    return num_primes;
}

int main(int argc, char *argv[]) {
    int quantum_usecs = 100000;
    
    unsigned int thread_count = 50;
    int *threads = new int[thread_count];

    // Init user thread library
    int ret = uthread_init(quantum_usecs);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    long counter = 0;
    int random_numbers[thread_count];

    for (int i = 0; i < thread_count+1; i++) {
        int tid = uthread_create(worker, nullptr);
        if ( tid != -1 )
        {
            threads[i] = tid;
            if ( i % 3 == 0 )
            {
                uthread_suspend( tid );
            }
        }
    }

    for (int i = 0; i < thread_count; i++) {
        if ( i % 3 == 0 )
        {
            uthread_resume(threads[i]);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        // Add thread result to global total
        int *return_value;
        uthread_join(threads[i], (void**)&return_value);
        
        // Deallocate thread result
        // cout << "return value: " << *return_value << endl;
        counter += *return_value;
        // cout << "join";
        delete return_value;
    }

    delete[] threads;

    cout << "Final result: " << counter << endl;

    return 0;
}
