#include "uthread.h"
#include <iostream>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    cout << "thread id: " << my_tid << endl;

    int prime_number_counter = 0;
    int *num_primes = new int;
    bool hasAFactor;

    for ( int i = 2; i < 10000; i++ )
    {
        hasAFactor = false;
        for ( int j = 2; j < i/2; j++ )
        {
            if ( i % j == 0 )
            {
                hasAFactor = true;
            }
        }
        if (!hasAFactor)
        {
            prime_number_counter++;
            // Force a third of the threads to exit at the 500th prime number
            if ( prime_number_counter == 500 && 
                 my_tid % 3 == 0 )
            {
                cout << "Thread " << my_tid << " exiting early" << endl;
                *num_primes = prime_number_counter;
                uthread_exit( num_primes );
            }
        }

        if ( i % 50 == 0 )
        {
            cout << "[" << my_tid << "]" << " Quantums: " << uthread_get_quantums( my_tid ) << endl;
            cout << "[" << my_tid << "]" << " Total Quantums: " << uthread_get_total_quantums( ) << endl;
        }
    }

    *num_primes = prime_number_counter;
    return num_primes;
}

int main(int argc, char *argv[]) {
    int quantum_usecs = 1000;
    
    unsigned int thread_count = 20;
    int *threads = new int[thread_count];

    // Init user thread library
    int ret = uthread_init(quantum_usecs);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    long counter = 0;
    int random_numbers[thread_count];

    // Attempt to create more threads than are allowed (99 plus main thread).
    // Should see one error message
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
        
        counter += *return_value;
        cout << "Thread " << threads[i] << " returned " << *return_value << endl;
        // Deallocate thread result
        delete return_value;
    }

    delete[] threads;

    cout << "Final result: " << counter << endl;

    return 0;
}
