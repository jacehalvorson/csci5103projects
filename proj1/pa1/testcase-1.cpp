#include "uthread.h"
#include <iostream>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    int parameter = *(int*)arg;

    printf("thread id: %d, parameter: %d\n", my_tid, parameter);
    unsigned long return_zero = 0;
    unsigned long *return_buffer = new unsigned long;

    *return_buffer = my_tid;

    return return_buffer;
}

int main(int argc, char *argv[]) {
    int quantum_usecs = 1000;
    
    unsigned int thread_count = 5;
    int *threads = new int[thread_count];

    // Init user thread library
    int ret = uthread_init(quantum_usecs);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    srand(time(NULL));
    //unsigned int random_parameter = 0;
    
    int *random_numbers = (int *)malloc(thread_count * sizeof(int));

    // Create threads
    for (int i = 0; i < thread_count; i++) {
        random_numbers[i] = rand();
        printf("random number generated: %d\n", random_numbers[i]);
        int tid = uthread_create(worker, &random_numbers[i]);
        threads[i] = tid;
    }

    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        // Add thread result to global total
        unsigned long *return_value;
        uthread_join(threads[i], (void**)&return_value);
        //
        // Deallocate thread result
        delete return_value;
    }

    delete[] threads;

    return 0;
}
