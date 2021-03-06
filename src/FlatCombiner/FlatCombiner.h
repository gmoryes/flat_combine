#ifndef FLAT_COMBINE_FLATCOMBINER_H
#define FLAT_COMBINE_FLATCOMBINER_H

#include <pthread.h>
#include <functional>
#include <atomic>
#include <sstream>
#include <thread>
#include <mutex>
#include <iostream>
#include <array>

#include "ThreadLocal.h"
#include "TaggedPointer.h"

namespace FlatCombiner {

/**
 * Extend user-defined pending operation type with fields required for the
 * flat combine algorithm to work
 * @tparam OpNode - type of user-defined operation slot
 */
template <typename OpNode>
class Operation : public OpNode {
public:
    /* Just beauty op_code for NOT_SET */
    enum {
        NOT_SET
    };

    Operation ():
        OpNode(),
        generation(0),
        next_and_alive(nullptr),
        op_code(NOT_SET)
    {}

    /* When last time this slot was detected as been in use */
    uint64_t generation;

    /* Pointer to the next slot */
    TaggedPointer<Operation<OpNode>> next_and_alive;

    /* The code of current execute operation (zero means not set) */
    std::atomic<int> op_code;

    /**
     * @return true if slot has operation to perform
     */
    bool has_data(std::memory_order read) {
        return op_code.load(read) != NOT_SET;
    }

    /**
     * @return pointer to next slot
     */
    Operation *next(std::memory_order read) {
        return next_and_alive.get(read);
    }

    /**
     * @return true if slot is still in queue, check that next_and_alive non zero
     */
    bool in_queue(std::memory_order read) const {
        return next_and_alive.get(read) != nullptr;
    }

    /**
     * @return true is slot is alive
     */
    bool is_alive(std::memory_order read) const {
        return next_and_alive.is_flag_set(read);
    }
};

/**
 * Create new flat combine synchronization primitive
 *
 * @template_param OpNode
 * Class for a single pending operation descriptor. Must provides following API:
 * - execute(std::array<std::pair<OpNode*, int>> tasks, size_t n) - function, that passes tasks to shared
 *   structure, in array stored pointer to user-defined slots and op_code for each of them. The second
 *   argument - amount of tasks in array.
 *
 * @template_param SHOT_N
 * Maximum array size that could be passed to a single Combine function call
 */
template <typename OpNode, size_t SHOT_N = 64>
class FlatCombiner {
public:
    // User defined type for the pending operations description
    using pending_operation = OpNode;

    FlatCombiner():
        _lock(0),
        _slot(nullptr, std::function<void(void*)>(slot_destructor)) {

        // Create Head of lock-free queue
        auto dummy_slot_head = new Operation<OpNode>();
        _queue.store(dummy_slot_head);

        // Create Tail of lock-free queue
        auto dummy_slot_tail = new Operation<OpNode>();
        _dummy_tail = dummy_slot_tail;

        // Head and Tail are dummy slots, that useful for preserve invariant, that
        // all slots have next slot, if not that means they were dequeued.

        // Push Tail after Head
        FlatCombiner::push_to_queue(dummy_slot_tail);
    }

    ~FlatCombiner() {
        // All threads should do detach() before FlatCombiner destructor call (!)
        delete _dummy_tail;
        delete get_queue_head();
    }

    /**
     * @return pending operation slot to the calling thread, object stays valid as long
     * as current thread is alive or got called detach method
     */
    OpNode *get_slot() {

        Operation<OpNode> *slot = _slot.get();

        if (slot == nullptr) {
            slot = new Operation<OpNode>();
            _slot.set(slot);
        }

        return slot;
    }

    /**
     * Put pending operation in the queue and try to execute it. Method gets blocked until
     * slot gets complete, in other words until slot.is_complete() returns false
     *
     * @param op_code_ - Code of operation to execute
     */
    void apply_slot(int op_code) {

        // Get thread local slot
        Operation<OpNode> *slot = _slot.get();
        if (slot == nullptr)
            throw std::runtime_error("Received nullptr slot");

        // Drop some flags and show other threads, that we have op_code
        slot->op_code.store(op_code, std::memory_order_release);

        // Always get newest version of slot to find out that slot is done
        while (slot->has_data(std::memory_order_acquire)) {

            // Need check in each loop
            if (not slot->in_queue(std::memory_order_acquire))
                FlatCombiner::push_to_queue(slot);

            // Try lock
            if (uint64_t generation = FlatCombiner::try_lock()) {
                // Yep!! We are executor

                // Call executor
                FlatCombiner::run_executor(generation);
                FlatCombiner::unlock();

                return;
            } else {

                // We're looser, try yield and do something usefull
                std::this_thread::yield();
            }
        }
    }

    /**
     * Detach calling thread from this flat combiner, in other word
     * destroy thread slot in the queue
     */
    void detach() {
        Operation<OpNode> *slot = _slot.get();
        if (slot != nullptr) {
            _slot.set(nullptr);
        } else {
            return;
        }

        orphan_slot(slot);
    }

    double get_stat() {
        return sum / sum_n;
    }

protected:

     /**
      * Try to acquire "lock", in case of success returns current generation. If
      * fails the return 0
      * @param suc - memory barrier in case of acquire lock
      * @param fail - memory barrier in case of fail to get lock
      * @return current generation
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
     * Beauty try_lock(suc, fail) without barriers
     * @return current genereation
     */
    uint64_t try_lock() {
        return FlatCombiner::try_lock(std::memory_order_release, std::memory_order_relaxed);
    }

    /**
     * Do unlock()
     */
    void unlock() {
        uint64_t lock = _lock.load(std::memory_order_acquire);
        lock &= ~LOCK_BIT_MASK;
        _lock.store(lock, std::memory_order_release);
    }

     /**
      * Remove slot from the queue. Note that method must be called only
      * under "lock" to eliminate concurrent queue modifications
      * @param parent - parent of "need_remove" slot
      * @param need_remove - slot to be removed from queue
      * @return need_remove.is_alive == is_alive (can be not equal in case of concurrent access)
      */
    bool dequeue_slot(Operation<OpNode> *parent, Operation<OpNode> *need_remove, bool is_alive) {

        // Get next->next
        auto new_next = need_remove->next(std::memory_order_relaxed);

        // Change next slot with CAS
        //auto copy_expected_value = need_remove;
        while (not parent->next_and_alive.compare_exchange_strong(
            need_remove,
            new_next,
            std::memory_order_release,
            std::memory_order_acquire)) {

            // Get newest version of parent
            parent = parent->next(std::memory_order_acquire);
        }

        return need_remove->next_and_alive.update_value_with_tag_check(nullptr, is_alive);
    }

    /**
     * Function called once thread owning this slot is going to die or to
     * destroy slot in some other way
     *
     * @param slot_to_delete pointer to slot to be orphaned
     */
    static void orphan_slot(Operation<OpNode> *slot_to_delete) {

        if (not slot_to_delete->in_queue(std::memory_order_acquire)) {
            // If slot not in queue, the only reference to it
            // was in ThreadLocal, can safely delete it
            delete slot_to_delete;
            return;
        }

        // Else if we are in queue, let say other threads, that we are
        // dead, executor will delete pointer
        slot_to_delete->next_and_alive.unset_flag(std::memory_order_relaxed, std::memory_order_release);
    }

    /**
     * Function to be passed as destructor for ThreadLocal
     * @param pointer
     */
    static void slot_destructor(void *pointer) {
        orphan_slot(static_cast<Operation<OpNode>*>(pointer));
    }

private:
    /* Bit of lock state */
    static constexpr uint64_t LOCK_BIT_MASK = uint64_t(1) << 63L;

    /* Mask to get generation */
    static constexpr uint64_t GEN_VAL_MASK = ~LOCK_BIT_MASK;

    /*
     * The time (count in generation units) we save slot
     * in queue, after it expired we dequeue it
     */
    static constexpr uint64_t MAX_AWAIT_TIME = 64;

    /*
     * First bit is used to see if lock is acquired already or no. Rest of bits is
     * a counter showing how many "generation" has been passed. One generation is a
     * single call of flat_combine function.
     */
    std::atomic<uint64_t> _lock{};

    /*
     * Head of flat combiner lock-free queue.
     * Note, that head and tail are dummy slots, that helps us to
     * preserve the invariant, that slot in queue always(!) has
     * next, if it's not, that means it isn't in queue.
     */
    std::atomic<Operation<OpNode>*> _queue{};
    Operation<OpNode> *_dummy_tail{};

    /* Array of operation, to be passed to shared structure */
    std::array<Operation<OpNode>*, SHOT_N> _combine_shot;

    /* ThreadLocal slot for each thread */
    ThreadLocal<Operation<OpNode>> _slot;

    /**
     * Insert between dummy slot - Head (_queue) and next to it
     * @param new_node - slot to be pushed
     */
    void push_to_queue(Operation<OpNode> *new_node) {

        // Get Head of queue
        auto head = get_queue_head();
        Operation<OpNode> *next;

        // Do CAS of head->next, and new_node
        next = head->next_and_alive.get(std::memory_order_relaxed);
        do {
            new_node->next_and_alive.set(next);
        } while (!head->next_and_alive.compare_exchange_weak(
            next,
            new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    /**
     * Function that run the winner of try_lock() (executor)
     * @param generation - current generation, for dequeue old slots
     */
    void run_executor(uint64_t generation) {

        auto parent = get_queue_head();

        // Get the newest version of parent->next
        auto current_node = parent->next_and_alive.get(std::memory_order_acquire);

        size_t n = 0;

        // Scan queue until get dummy tail
        while (current_node != _dummy_tail) {

            // Don't work with detached() slots
            if (current_node->is_alive(std::memory_order_relaxed)) {

                // If current slot has data, add it to request array
                if (current_node->has_data(std::memory_order_acquire)) {

                    current_node->generation = generation;
                    _combine_shot[n] = current_node;
                    n++;
                    parent = current_node;

                // Check if slot has expired
                } else if (generation - current_node->generation > MAX_AWAIT_TIME) {

                    bool was_alive = FlatCombiner::dequeue_slot(parent, current_node, /* is_alive = */ true);
                    if (!was_alive) {
                        // We thought, that current_node was alive, but because of concurrent access it's not
                        // So the last pointer here, we can safely delete it.
                        delete current_node;
                    }

                    // For call next_and_alive from parent, slot->next_and_alive is zero after dequeue_slot()
                    current_node = parent;
                } else {
                    parent = current_node;
                }

            } else {
                // In this case slot has detached()

                bool was_dead = FlatCombiner::dequeue_slot(parent, current_node, /* is_alive = */ false);

                // Must be always dead
                assert(was_dead);

                // Only owner of this ThreadLocal variable could unset the alive flag, if it zero
                // here the only reference to it, so we safely delete after change Next of parent
                delete current_node;

                // For call next_and_alive from parent, slot->next_and_alive is zero now
                current_node = parent;
            }

            current_node = current_node->next_and_alive.get(std::memory_order_acquire);
        }

        try {
            // Do combine shot (execute all operations stored by executor)
            FlatCombiner::combine_shot(_combine_shot, n);
            sum_n++;
            sum += n;
        } catch (std::exception &exception) {
            std::cerr << "Exception after combine_shot(): " << exception.what() << std::endl;
        }
    }

    /**
     * Execute operations stored by executor
     * @param executor_tasks - list of tasks
     * @param n - amount of tasks
     */
    static void combine_shot(std::array<Operation<OpNode>*, SHOT_N> &executor_tasks, size_t n) {

        // Reorganize array, store additionally op_code for shared structure
        std::array<std::pair<OpNode*, int>, SHOT_N> user_tasks;
        for (size_t i = 0; i < n; i++) {
            user_tasks.at(i) = std::make_pair(
                executor_tasks[i],
                executor_tasks[i]->op_code.load(std::memory_order_relaxed)
            );
        }

        // Call user defined execute of operation
        executor_tasks.at(0)->template execute<SHOT_N>(user_tasks, n);

        // Update some flags for flat combiner
        for (size_t i = 0; i < n; i++) {
            executor_tasks.at(i)->op_code.store(Operation<OpNode>::NOT_SET, std::memory_order_release);
        }
    }

    inline Operation<OpNode>* get_queue_head() const {
        return _queue.load(std::memory_order_relaxed);
    }

    double sum;
    double sum_n;
};

} // namespace FlatCombiner

#endif //FLAT_COMBINE_FLATCOMBINER_H
