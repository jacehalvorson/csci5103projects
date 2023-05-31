CC = g++
CFLAGS = -lrt -g
DEPS = TCB.h uthread.h
OBJ0 = TCB.o uthread.o main.o
OBJ = TCB.o uthread.o testcase-4.o
OBJ2 = TCB.o uthread.o testcase-5.o
OBJ3 = TCB.o uthread.o testcase-6.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

pi: $(OBJ0)
	$(CC) -o $@ $^ $(CFLAGS)

testcase4: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

testcase5: $(OBJ2)
	$(CC) -o $@ $^ $(CFLAGS)

testcase6: $(OBJ3)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f pi testcase4 testcase5 testcase6 *.o
