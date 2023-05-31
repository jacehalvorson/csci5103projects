#include "TCB.h"
using namespace std;

TCB::TCB(int tid, void *(*start_routine)(void* arg), void *arg, State state)
{
    _tid = tid;
    _quantum = 0;
    _state = state;
    getcontext(&_context);
    if (tid > 0) {
        _stack = new char[STACK_SIZE];
        _context.uc_stack.ss_sp = _stack;
        _context.uc_stack.ss_size = STACK_SIZE;
        _context.uc_stack.ss_flags = 0;
        makecontext(&_context, (void(*)())stub, 2, start_routine, arg);
    }
}

TCB::~TCB()
{
    delete[] (char*)_context.uc_stack.ss_sp;
}

void TCB::setState(State state)
{
    this->_state = state;
}

State TCB::getState() const
{
    return this->_state;
}

int TCB::getId() const
{
    return this->_tid;
}

void TCB::increaseQuantum()
{
    _quantum++;
}

int TCB::getQuantum() const
{
    return this->_quantum;
}

int TCB::saveContext()
{
    return getcontext(&_context);
}

void TCB::loadContext()
{
    setcontext(&_context);
}
