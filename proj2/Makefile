CC = g++
CFLAGS = -g -lrt --std=c++14
DEPS = TCB.h uthread.h uthread_private.h Lock.h CondVar.h SpinLock.h
OBJ = TCB.o uthread.o Lock.o CondVar.o SpinLock.o
MAIN_OBJ = main.o
# MAIN_OBJ2 = lock-testcase.o
MAIN_OBJ3 = locks-testcase-bank.o
MAIN_OBJ4 = lock-performance.o
MAIN_OBJ5 = spinlock-performance.o
MAIN_OBJ6 = condvar-testcase-buffer.o
MAIN_OBJ7 = priority-testcase.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

uthread-sync-demo: $(OBJ) $(MAIN_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

locks-testcase-bank: $(OBJ) $(MAIN_OBJ3)
	$(CC) -o $@ $^ $(CFLAGS)

lock-performance: $(OBJ) $(MAIN_OBJ4)
	$(CC) -o $@ $^ $(CFLAGS)

spinlock-performance: $(OBJ) $(MAIN_OBJ5)
	$(CC) -o $@ $^ $(CFLAGS)

condvar-testcase-buffer: $(OBJ) $(MAIN_OBJ6)
	$(CC) -o $@ $^ $(CFLAGS)

priority-testcase: $(OBJ) $(MAIN_OBJ7)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *-testcase *-performace *-buffer *-demo *-bank
