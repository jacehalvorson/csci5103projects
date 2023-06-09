#include "uthread.h"
#include <iostream>
#include <unistd.h>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    int points_per_thread = *(int*)arg;

    unsigned long local_cnt = 0;
    unsigned int rand_state = rand();
    for (int i = 0; i < points_per_thread; i++) {
        if (i == points_per_thread / 10 == 0) {
            uthread_yield();
        }
        double x = rand_r(&rand_state) / ((double)RAND_MAX + 1) * 2.0 - 1.0;
        double y = rand_r(&rand_state) / ((double)RAND_MAX + 1) * 2.0 - 1.0;
        if (x * x + y * y < 1)
            local_cnt++;
    }

    // NOTE: Parent thread must deallocate
    unsigned long *return_buffer = new unsigned long;
    *return_buffer = local_cnt;
    return return_buffer;
}

int main(int argc, char *argv[]) {
    // Default to 1 ms time quantum
    int quantum_usecs = 1000;

    if (argc < 3) {
        cerr << "Usage: ./pi <total points> <threads> [quantum_usecs]" << endl;
        cerr << "Example: ./pi 100000000 8" << endl;
        exit(1);
    } else if (argc == 4) {
        quantum_usecs = atoi(argv[3]);
    }
    unsigned long totalpoints = atol(argv[1]);
    int thread_count = atoi(argv[2]);

    int *threads = new int[thread_count];
    int points_per_thread = totalpoints / thread_count;

    // Init user thread library
    cout << "Initializing thread library" << endl;
    int ret = uthread_init(quantum_usecs);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    srand( clock( ) );

    // Create threads
    for (int i = 1; i < thread_count+1; i++) {
        cout << "Creating thread #" << i << endl;
        int tid = uthread_create(worker, &points_per_thread);
        if ( tid == -1 )
        {
            cerr << "Unable to create thread" << endl;
            return 1;
        }
        threads[i] = tid;
    }
    
    // Wait for all threads to complete
    unsigned long g_cnt = 0;
    for (int i = 1; i < thread_count+1; i++) {
        // Add thread result to global total
        unsigned long *local_cnt = nullptr;
        cout << "Joining thread #" << threads[i] << endl;
        uthread_join(threads[i], (void**)&local_cnt);

        if ( local_cnt != nullptr )
        {
            cout << "Thread #" << threads[i] << " returned " << *local_cnt << endl;
            g_cnt += (unsigned long) *local_cnt;
        }

        // Deallocate thread result
        delete local_cnt;
    }

    delete[] threads;

    cout << "Pi: " << (4. * (double)g_cnt) / ((double)points_per_thread * thread_count) << endl;

    return 0;
}
