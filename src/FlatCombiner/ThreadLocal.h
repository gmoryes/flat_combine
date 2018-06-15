#ifndef FLAT_COMBINE_THREADLOCAL_H
#define FLAT_COMBINE_THREADLOCAL_H

/**
 * Create new thread local variable
 */

#include <pthread.h>

template <typename T> class ThreadLocal {
public:
    explicit ThreadLocal(T *initial = nullptr, std::function<void(void*)> destructor = nullptr) {
        if (destructor != nullptr) {
            pthread_key_create(&_th_key, *destructor.target<void(*)(void*)>());
        } else {
            pthread_key_create(&_th_key, NULL);
        }
        set(initial);
    }

    inline T *get() {
        void *result;
        result = pthread_getspecific(_th_key);

        return static_cast<T*>(result);
    }

    inline void set(T *value) {
        int success = pthread_setspecific(_th_key, static_cast<void*>(value));
        if (success) {
            std::stringstream ss;
            ss << "Error during pthread_setspecific(), return: " << success;
            throw std::runtime_error(ss.str());
        }
    }

    T &operator*() {
        auto result = get();
        if (result == nullptr)
            throw std::runtime_error("Try to do dereference a null pointer");

        return *result;
    }

private:
    pthread_key_t _th_key;
};

#endif //FLAT_COMBINE_THREADLOCAL_H
