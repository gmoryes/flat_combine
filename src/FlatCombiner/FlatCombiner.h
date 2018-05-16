#ifndef FLAT_COMBINE_FLATCOMBINER_H
#define FLAT_COMBINE_FLATCOMBINER_H

#include <pthread.h>
#include <functional>
#include <atomic>
#include <sstream>
#include <thread>
#include <iostream>

/**
 * Create new thread local variable
 */
template <typename T> class ThreadLocal {
public:
    ThreadLocal(T *initial = nullptr, std::function<void(void*)> destructor = nullptr) {
        if (destructor != nullptr)
            pthread_key_create(&_th_key, *destructor.target<void(*)(void*)>());
        else
        pthread_key_create(&_th_key, NULL);
        set(initial);
    }

    inline T *get() {
        void *result;
        result = pthread_getspecific(_th_key);
        if (result == nullptr)
            return static_cast<T*>(result);

        return static_cast<T*>(result);
    }

    inline void set(T *value) {
        int success = pthread_setspecific(_th_key, static_cast<void*>(value));
        if (success) {
            std::stringstream ss;
            ss << "Error during pthread_setspecific(), return" << success;
            throw std::runtime_error(ss.str());
        }
    }

    T &operator*() { return *get(); }

private:
    pthread_key_t _th_key;

    static void default_destructor(void *ptr) {
        delete reinterpret_cast<T*>(ptr);
    }
};

/**
 * Create new flat combine synchronizaion primitive
 *
 * @template_param OpNode
 * Class for a single pending operation descriptor. Must provides following API:
 * - complete() returns true is operation gets completed and false otherwise
 * - error(const std::exception &ex) set operation as failed. After this call return,
 *   subsequent calls to complete() method must return true
 *
 * @template_param QMS
 * Maximum array size that could be passed to a single Combine function call
 */
template <typename SharedStructure, typename OpNode, std::size_t QMS = 64> class FlatCombiner {
public:
    // User defined type for the pending operations description, must be plain object without
    // virtual functions
    using pending_operation = OpNode;

    // Function that combine multiple operations and apply it onto data structure
    using combiner = std::function<void(OpNode *, OpNode *)>;

    // Maximum number of pending operations could be passed to a single Combine call
    static const std::size_t max_call_size = QMS;

    // Extend user provided pending operation type with fields required for the
    // flat combine algorithm to work
    using Slot = struct Slot {
        static constexpr uint64_t THREAD_ALIVE_BIT = uint64_t(1) << 63L;
        static constexpr uint64_t CLEAR_THREAD_ALIVE_BIT = ~THREAD_ALIVE_BIT;

        Slot (std::shared_ptr<SharedStructure> storage):
            user_op(storage),
            generation(0),
            next_and_alive(0),
            expired(false) {}

        // User pending operation to be complete
        OpNode user_op;

        // When last time this slot was detected as been in use
        uint64_t generation;

        // Pointer to the next slot. One bit of pointer is stolen to
        // mark if owner thread is still alive, based on this information
        // combiner/thread_local destructor able to take decission about
        // deleting node.
        //
        // So if stolen bit is set then the only reference left to this slot
        // if the queue. If pointer is zero and bit is set then the only ref
        // left is thread_local storage. If next is zero there are no
        // link left and slot could be deleted
        std::atomic<uint64_t> next_and_alive;

        std::atomic<bool> expired;
        /**
         * Remove alive bit from the next_and_alive pointer and return
         * only correct pointer to the next slot
         */
        Slot *next() {
            return Slot::get_raw_pointer(next_and_alive.load(std::memory_order_acquire));
        }

        static Slot *get_raw_pointer(uintptr_t ptr) {
            return reinterpret_cast<Slot*>(ptr & CLEAR_THREAD_ALIVE_BIT);
        }

        static bool is_alive(uintptr_t ptr) {
            return (ptr & THREAD_ALIVE_BIT) != 0;
        }
    };

    /**
     * @param Combine function that aplly pending operations onto some data structure. It accepts array
     * of pending ops and allowed to modify it in any way except delete pointers
     */ /* _slot(nullptr, orphan_slot) */
    FlatCombiner(combiner combine):
        _slot(nullptr),
        _combine(combine) {

        _lock.store(0, std::memory_order_relaxed);
        _data_structure = std::make_shared<SharedStructure>();
        auto dummy_slot = new Slot(_data_structure);
        auto result_ptr_modify = reinterpret_cast<uintptr_t>(dummy_slot);
        _queue.store(result_ptr_modify, std::memory_order_relaxed);
    }
    ~FlatCombiner() { /* dequeue all slot, think about slot deletition */ }

    /**
     * Return pending operation slot to the calling thread, object stays valid as long
     * as current thread is alive or got called detach method
     */
    pending_operation *get_slot() {

        uintptr_t *modified_ptr = _slot.get();
        Slot *slot;

        if (modified_ptr == nullptr) {
            slot = new Slot(_data_structure);
            std::cout << "recv slot(" << slot << ")" << std::endl;
            auto *result_ptr_modify = new uintptr_t(reinterpret_cast<uintptr_t>(slot) | Slot::THREAD_ALIVE_BIT);
            _slot.set(result_ptr_modify);
            std::cout << "push to queue(" << slot << ")" << std::endl;
            FlatCombiner::push_to_queue(*result_ptr_modify);
        } else {
            auto result_ptr_modify = reinterpret_cast<uintptr_t>(_slot.get());
            slot = Slot::get_raw_pointer(result_ptr_modify);
        }

        return &(slot->user_op);
    }

    /**
     * Put pending operation in the queue and try to execute it. Method gets blocked until
     * slot gets complete, in other words until slot.complete() returns false
     */
    void apply_slot() {
        // TODO: assert slot params
        // TODO: enqueue slot if needs
        // TODO: try to become executor (acquire lock)
        //       TODO: scan queue, dequeue stale nodes, prepare array to be passed to Combine call
        //       TODO: call Combine function
        //       TODO: unlock
        // TODO: if lock fails, do thread_yeild and goto 3 TODO
        uintptr_t modified_ptr = *_slot.get();
        Slot *slot = Slot::get_raw_pointer(modified_ptr);

        if (slot->expired.load(std::memory_order_acquire)) {
            std::cout << "slot(" << (uintptr_t)slot << ") has expired" << std::endl;
            FlatCombiner::push_to_queue(reinterpret_cast<uintptr_t>(modified_ptr));
            slot->expired.store(false, std::memory_order_relaxed);
        }

        while (true) {
            if (uint64_t generation = FlatCombiner::try_lock(std::memory_order_release, std::memory_order_relaxed)) {
                // Yep!! We are executor
                std::cout << "slot(" << slot << ") is executor" << std::endl;
                FlatCombiner::run_executor(generation);
                FlatCombiner::unlock();
                std::cout << "slot(" << slot << ") unlock" << std::endl;
                return;
            } else {
                // :((
                if (slot->user_op.complete())
                    return;

                std::this_thread::yield();
            }
        }
    }

    /**
     * Detach calling thread from this flat combiner, in other word
     * destroy thread slot in the queue
     */
    void detach() {
        pending_operation *result = _slot.get();
        if (result != nullptr) {
            _slot.set(nullptr);
        }
        orphan_slot(result);
    }

protected:
    /**
     * Try to acquire "lock", in case of success returns current generation. If
     * fails the return 0
     *
     */
    uint64_t try_lock(std::memory_order suc, std::memory_order fail) {
        uint64_t lock = _lock.load(std::memory_order_acquire);
        if (lock & LOCK_BIT_MASK)
            return 0;

        uint64_t new_lock = LOCK_BIT_MASK | ((lock & GEN_VAL_MASK) + 1);
        bool result = _lock.compare_exchange_weak(lock, new_lock, suc, fail);
        if (result)
            return new_lock & GEN_VAL_MASK;
        else
            return 0;
    }

    /**
     * Try to release "lock". Increase generation number in case of success
     *
     * @param suc memory barrier to set in case of success lock
     * @param fail memory barrier to set in case of failure
     */
    void unlock() {
        uint64_t lock = _lock.load(std::memory_order_acquire);
        lock &= ~LOCK_BIT_MASK;
        _lock.store(lock, std::memory_order_release);
    }

    /**
     * Remove slot from the queue. Note that method must be called only
     * under "lock" to eliminate concurrent queue modifications
     *
     */
    void dequeue_slot(Slot *parent, Slot *need_remove) {
        // TODO: remove node from the queue
        // TODO: set pointer pare of "next" to null, DO NOT modify usage bit
        // TODO: if next == 0, delete pointer
        parent->next_and_alive.store(need_remove->next_and_alive, std::memory_order_relaxed);
        need_remove->next_and_alive.store(0, std::memory_order_relaxed);
    }

    /**
     * Function called once thread owning this slot is going to die or to
     * destory slot in some other way
     *
     * @param Slot pointer to the slot is being to orphan
     */
    void orphan_slot(Slot *) {}

private:
    static constexpr uint64_t LOCK_BIT_MASK = uint64_t(1) << 63L;
    static constexpr uint64_t GEN_VAL_MASK = ~LOCK_BIT_MASK;
    static constexpr uint64_t MAX_AWAIT_TIME = 64;

    // First bit is used to see if lock is acquired already or no. Rest of bits is
    // a counter showing how many "generation" has been passed. One generation is a
    // single call of flat_combine function.
    //
    // Based on that counter stale slots found and gets removed from the pending
    // operations queue
    std::atomic<uint64_t> _lock{};

    // Pending operations queue. Each operation to be applied to the protected
    // data structure is ends up in this queue and then executed as a batch by
    // flat_combine method call
    std::atomic<uintptr_t> _queue{};

    // Insert between dummy slot and next to it
    void push_to_queue(uintptr_t new_node_ptr) {

        auto head = reinterpret_cast<Slot*>(_queue.load(std::memory_order_relaxed));
        auto new_node = Slot::get_raw_pointer(new_node_ptr);

        uintptr_t next;
        std::cout << "Before: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
        std::cout << ", current(" << new_node_ptr << ")" << std::endl;
        do {
            next = head->next_and_alive.load(std::memory_order_acquire);
            new_node->next_and_alive.store(next, std::memory_order_relaxed);
        } while (!head->next_and_alive.compare_exchange_weak(
            next,
            new_node_ptr,
            std::memory_order_release,
            std::memory_order_relaxed));
        std::cout << "After: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
        std::cout << ", next(" << new_node->next_and_alive.load(std::memory_order_relaxed);
        std::cout << ")" << std::endl;
        /*
         * !std::atomic_compare_exchange_weak(
            &head->next_and_alive,
            &new_node->next_and_alive,
            new_node_ptr,
            std::memory_order_release,
            std::memory_order_relaxed)
         */

    }

    // Function to call in order to execute operations
    combiner _combine;

    // Usual strategy for the combine flat would be sort operations by some creteria
    // and optimize it somehow. That array is using by executor thread to prepare
    // number of ops to pass to combine
    // TODO Зачем ограничивать? Не получится ли так, что какие-то треды будут постоянно ждать своей очереди
    std::array<OpNode *, QMS> _combine_shot;

    // Slot of the current thread. If nullptr then cur thread gets access in the
    // first time or after a long period when slot has been deleted already
    ThreadLocal<uintptr_t> _slot;

    void run_executor(uint64_t generation) {

        auto parent = reinterpret_cast<Slot*>(_queue.load(std::memory_order_relaxed));
        uintptr_t current_node = parent->next_and_alive.load(std::memory_order_relaxed);

        int n = 0;
        while (current_node) {
            std::cout << "get current_node(" << current_node << ")" << std::endl;
            Slot *slot = Slot::get_raw_pointer(current_node);
            if (Slot::is_alive(current_node)) {
                if (slot->user_op.has_data()) {
                    std::cout << "slot(" << slot << ") has data" << std::endl;
                    slot->generation = generation;
                    _combine_shot[n] = &slot->user_op;
                    n++;
                    parent = slot;
                } else if (generation - slot->generation > MAX_AWAIT_TIME) {
                    std::cout << "gen of slot(" << current_node << ") = " << slot->generation;
                    std::cout << " current = " << generation << std::endl;
                    slot->expired.store(true, std::memory_order_release);
                    FlatCombiner::dequeue_slot(parent, slot);
                }
            } else {
                FlatCombiner::dequeue_slot(parent, slot);
            }

            current_node = slot->next_and_alive;
        }

        // TODO fix error
        //_combine(_combine_shot.data(), _combine_shot.data() + n);
        for (int i = 0; i < n; i++) {
            _combine_shot[i]->execute();
        }
    }

    std::shared_ptr<SharedStructure> _data_structure;
};

#endif //FLAT_COMBINE_FLATCOMBINER_H
