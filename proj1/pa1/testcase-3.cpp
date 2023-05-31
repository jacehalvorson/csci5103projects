#include "uthread.h"
#include <iostream>

using namespace std;

void *worker(void *arg) {
    int my_tid = uthread_self();
    int parameter = *(int*)arg;

    printf("thread id: %d, parameter: %d\n", my_tid, parameter);
    unsigned long return_zero = 0;
    int *return_buffer = new int;
    
    
    if (parameter > my_tid){
      *return_buffer = parameter - my_tid;
    }else{
      *return_buffer = my_tid - parameter;
    }

    for(int i = 0; i < 10000000 + *return_buffer; i++){
      printf("threads %d\n", my_tid);
    }

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
        int *return_value;
        uthread_join(threads[i-1], (void**)&return_value);
        //
        // Deallocate thread result
        printf("return value: %d\n", *return_value);
        delete return_value;
    }

    delete[] threads;

    return 0;
}
