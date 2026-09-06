#ifndef SIM_MESSAGEQUEUE_H
#define SIM_MESSAGEQUEUE_H

#include <SDL2/SDL_mutex.h>

namespace SIM {

template <class T>
class MessageQueue {
    T *data;
    unsigned read_pos;
    unsigned write_pos;
    signed int in_use;
    const unsigned capacity;
	SDL_sem* semaphore;
    SDL_mutex* mutex;
public:
    MessageQueue(unsigned size) :
        data((T *)operator new(size * sizeof(T))),
        read_pos(0),
        write_pos(0),
        in_use(0),
        capacity(size)
    {
        semaphore = SDL_CreateSemaphore(0);
        mutex = SDL_CreateMutex();
    }

    void SendMessage(T const &t) {
        SDL_LockMutex(mutex);
        // ensure there's room in buffer:
        //if (in_use == capacity) 
        //    pop();
        // spin until some capacity frees up
        //TODO:maybe dont spin
        while(in_use >= capacity) {
            //SDL_Delay(1);
        }

        // construct copy of object in-place into buffer
        new(&data[write_pos++]) T(t);
        // keep pointer in bounds.
        write_pos %= capacity;
        ++in_use;
        SDL_SemPost(semaphore);
        SDL_UnlockMutex(mutex);
    }

    // return oldest object in queue:
    T front() {
        return data[read_pos];
    }

    // remove oldest object from queue:
    void pop() { 
        // destroy the object:
        data[read_pos++].~T();

        // keep pointer in bounds.
        read_pos %= capacity;
        --in_use;
    }

    // Try to read the first message without popping it
    // Non blocking: if no message exists, returns false and msgOut is not written
    bool TryReadMessage(T& msgOut) {
        SDL_LockMutex(mutex);
        if(write_pos == read_pos) {
            SDL_UnlockMutex(mutex);
            return false;
        }
        T ret = data[read_pos];
        SDL_UnlockMutex(mutex);
        return ret;
    }
	
	T ReceiveMessage() {
        while(in_use == 0) {
            SDL_SemWait(semaphore);
        }

        SDL_LockMutex(mutex);
        T ret = data[read_pos];
        pop();
        SDL_UnlockMutex(mutex);
        return ret;
    }

    bool empty() {return (in_use == 0);}
  
~MessageQueue() {
    // first destroy any content
    while (in_use != 0)
        pop();

    SDL_DestroySemaphore(semaphore);
    SDL_DestroyMutex(mutex);

    // then release the buffer.
    operator delete(data); 
}

};
}

#endif
