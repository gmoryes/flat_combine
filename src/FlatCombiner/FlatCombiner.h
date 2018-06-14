#ifndef FLAT_COMBINE_FLATCOMBINER_H
#define FLAT_COMBINE_FLATCOMBINER_H

#include <pthread.h>
#include <functional>
#include <atomic>
#include <sstream>
#include <thread>
#include <mutex>
#include <iostream>
#include "ThreadLocal.h"
#include <array>
#include "../Logger/Logger.h"

namespace FlatCombiner {

// Extend user provided pending operation type with fields required for the
// flat combine algorithm to work
template <typename OpNode>
struct Operation {
    // Just beauty op_code for NOT_SET
    enum {
        NOT_SET
    };

    Operation ():
        user_op(),
        generation(0),
        next_and_alive(0),
        alive_flag(true),
        op_code(NOT_SET),
        complete(false),
        error(false)
    {}

    // User pending operation to be complete
    OpNode user_op;

    // When last time this slot was detected as been in use
    uint64_t generation;

    // Pointer to the next slot.
    std::atomic<Operation*> next_and_alive;

    // Slot alive flat
    std::atomic<bool> alive_flag;

    // The code of current execute operation (zero means not set)
    std::atomic<int> op_code;

    // Flag of completeness of operation
    std::atomic<bool> complete;

    // Did the last operation complete with error
    std::atomic<bool> error;

    // Save exeption to get it to user
    std::exception exception;

    // Return pointer to user's Operation Node
    OpNode *user_slot() {
        return &user_op;
    }

    // Prepare request data before execute it
    template <typename... Args>
    void set(int op_code_, Args... args) {
        // Call user defined function
        user_op.prepare_data(std::forward<Args>(args)...);

        // Drop some flags
        complete.store(false, std::memory_order_relaxed);
        error.store(false, std::memory_order_relaxed);
        op_code.store(op_code_, std::memory_order_release);
    }

    // Return true if operation complete, false otherwise
    bool is_complete() {
        return complete.load(std::memory_order_acquire);
    }

    // Return true if error happened, false otherwise
    int error_code() {
        return user_op.error_code;
    }

    // Check if slot has operation to date
    bool has_data() {
        return op_code.load(std::memory_order_acquire) != NOT_SET;
    }

    // Return pointer to next slot
    Operation *next() {
        return next_and_alive.load(std::memory_order_acquire);
    }

    // Return true if slot is still in queue, check that next_and_alive non zero
    bool in_queue() const {
        return next_and_alive.load(std::memory_order_acquire) != nullptr;
    }

    // Return true is slot is alive
    bool is_alive() const {
        return alive_flag.load(std::memory_order_acquire);
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
template <typename OpNode, size_t SHOT_N = 64>
class FlatCombiner {
public:
    // User defined type for the pending operations description, must be plain object without
    // virtual functions
    using pending_operation = OpNode;

    /**
     * @param Combine function that aplly pending operations onto some data structure. It accepts array
     * of pending ops and allowed to modify it in any way except delete pointers
     */
    FlatCombiner();

    ~FlatCombiner();

    /**
     * Return pending operation slot to the calling thread, object stays valid as long
     * as current thread is alive or got called detach method
     */
    Operation<OpNode> *get_slot();

    /**
     * Put pending operation in the queue and try to execute it. Method gets blocked until
     * slot gets complete, in other words until slot.complete() returns false
     */
    void apply_slot() {
        Operation<OpNode> *slot = _slot.get();
        if (slot == nullptr)
            throw std::runtime_error("Received nullptr");

        while (not slot->is_complete()) {

            //std::stringstream ss;
            //ss << "check in queue slot_1(" << slot << ")"; my_log(ss);
            /* In case:
             *   thread1: has_data() in slot ? Answer: no
             *   thread1: start dequeue_slot()
             *
             */
            if (not slot->in_queue())
                FlatCombiner::push_to_queue(slot);

            if (uint64_t generation = FlatCombiner::try_lock()) {
                // Yep!! We are executor

                //std::stringstream ss;
                //ss << "slot(" << slot << ") is executor"; my_log(ss);

//                std::stringstream ss0;
//                ss0 << "check in queue slot_2(" << slot << ")"; my_log(ss0);
                if (not slot->in_queue())
                    FlatCombiner::push_to_queue(slot);

                FlatCombiner::run_executor(generation);
                FlatCombiner::unlock();

//                std::stringstream ss1;
//                ss1 << "slot(" << slot << ") unlock" << std::endl; my_log(ss1);
                return;
            } else {
                // :((
                std::this_thread::yield();
            }
        }

//        std::stringstream ss2;
//        ss2 << "slot(" << slot << ") apply completed"; my_log(ss2);
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

protected:
    /**
     * Try to acquire "lock", in case of success returns current generation. If
     * fails the return 0
     *
     */
    //std::mutex mutex;
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

    uint64_t try_lock() {
        return FlatCombiner::try_lock(std::memory_order_release, std::memory_order_relaxed);
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
    void dequeue_slot(Operation<OpNode> *parent, Operation<OpNode> *need_remove) {
        // TODO: remove node from the queue
        // TODO: set pointer pare of "next" to null, DO NOT modify usage bit
        // TODO: if next == 0, delete pointer

//        std::stringstream ss1;
//        ss1 << "dequeue_slot(): slot(" << need_remove << "), parent(" << parent << ")"; my_log(ss1);
        auto new_next = need_remove->next();

//        std::stringstream ss2;
//        ss2 << "dequeue_slot(): new_next(" << new_next << ")"; my_log(ss2);

        auto copy_expected_value = need_remove;
        while (not parent->next_and_alive.compare_exchange_strong(
            need_remove,
            new_next,
            std::memory_order_release,
            std::memory_order_acquire)) {

            need_remove = copy_expected_value;
            parent = parent->next();
        }

//        std::stringstream ss3;
//        ss3 << "dequeue_slot(): cas done, parent is(" << parent << ")"; my_log(ss3);
        need_remove->next_and_alive.store(nullptr, std::memory_order_release);
    }

    /**
     * Function called once thread owning this slot is going to die or to
     * destory slot in some other way
     *
     * @param slot modified adress to the slot is being to orphan
     */
    static void orphan_slot(Operation<OpNode> *slot_to_delete) {

        if (not slot_to_delete->in_queue()) {
            // THe only reference to it was in ThreadLocal, can safely delete it
//            std::stringstream ss5;
//            ss5 << std::hex << "orphan_slot(" << slot_to_delete << ") delete"; my_log(ss5);
            delete slot_to_delete;
            return;
        }

//        std::stringstream ss5;
//        ss5 << std::hex << "orphan_slot(" << slot_to_delete << ") mark as dead"; my_log(ss5);
        // Say other threads, that we are dead, executor will delete pointer
        slot_to_delete->alive_flag.store(false, std::memory_order_release);
    }

    static void slot_destructor(void *pointer) {
        orphan_slot(static_cast<Operation<OpNode>*>(pointer));
    }

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
    std::atomic<Operation<OpNode>*> _queue{};
    Operation<OpNode> *_dummy_tail;

    // Insert between dummy slot and next to it
    void push_to_queue(Operation<OpNode> *new_node) {

        auto head = _queue.load(std::memory_order_relaxed);

        Operation<OpNode> *next;
//        std::stringstream ss;
//        ss << std::hex << "Before: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
//        ss << ", current(" << new_node << ")"; my_log(ss);

        do {
            next = head->next_and_alive.load(std::memory_order_acquire);
            new_node->next_and_alive.store(next, std::memory_order_relaxed);
        } while (!head->next_and_alive.compare_exchange_weak(
            next,
            new_node,
            std::memory_order_release,
            std::memory_order_acquire));

//        std::stringstream ss1;
//        ss1 << std::hex << "After: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
//        ss1 << ", next(" << new_node->next_and_alive.load(std::memory_order_relaxed);
//        ss1 << ")"; my_log(ss1);

    }

    // Usual strategy for the combine flat would be sort operations by some creteria
    // and optimize it somehow. That array is using by executor thread to prepare
    // number of ops to pass to combine
    std::array<Operation<OpNode>*, SHOT_N> _combine_shot;

    // Slot of the current thread. If nullptr then cur thread gets access in the
    // first time or after a long period when slot has been deleted already
    ThreadLocal<Operation<OpNode>> _slot;

    void run_executor(uint64_t generation) {

        auto parent = _queue.load(std::memory_order_relaxed);
        auto current_node = parent->next_and_alive.load(std::memory_order_relaxed);

        size_t n = 0;
        while (current_node != _dummy_tail) {
//            std::stringstream ss;
//            ss << "run_executor(" << generation << "): get current_node(" << std::hex << current_node << ")"; my_log(ss);

//            std::stringstream ss1;
//            ss1 << std::hex << "check slot for alive(" << current_node << ")"; my_log(ss1);
            if (current_node->is_alive()) {

//                std::stringstream ss1;
//                ss1 << std::hex << "slot alive_ok(" << current_node << ")"; my_log(ss1);
                if (current_node->has_data()) {

//                    std::stringstream ss;
//                    ss << "slot(" << current_node << ") has data"; my_log(ss);
                    current_node->generation = generation;
                    _combine_shot[n] = current_node;
                    n++;
                    parent = current_node;

                } else if (generation - current_node->generation > MAX_AWAIT_TIME) {

//                    std::stringstream ss;
//                    ss << "gen of slot(" << current_node << ") = " << current_node->generation;
//                    ss << " current = " << generation; my_log(ss);
                    FlatCombiner::dequeue_slot(parent, current_node);

                    // For call next_and_alive from parent, slot->next_and_alive is zero now
                    current_node = parent;
                } else {
                    parent = current_node;
                }

            } else {
//                std::stringstream ss1;
//                ss1 << std::hex << "slot alive_bad(" << current_node << ")"; my_log(ss1);
                FlatCombiner::dequeue_slot(parent, current_node);

                // Only owner of this ThreadLocal variable could unset the alive flag, if it zero
                // the only reference to it in parent, so we safely delete after change Next of parent
//                std::stringstream ss;
//                ss << std::hex << "delete(" << current_node << ")_2"; my_log(ss);
                delete current_node;

                // For call next_and_alive from parent, slot->next_and_alive is zero now
                current_node = parent;
            }

            current_node = current_node->next_and_alive.load(std::memory_order_acquire);
        }

        // Do combine shot (execute all operations stored by executor)
        FlatCombiner::combine_shot(_combine_shot, n);
    }

    static void combine_shot(std::array<Operation<OpNode>*, SHOT_N> &executor_tasks, size_t n) {

        std::array<std::pair<OpNode*, int>, SHOT_N> user_tasks;
        for (size_t i = 0; i < n; i++) {
            user_tasks.at(i) = std::make_pair(
                &executor_tasks[i]->user_op,
                executor_tasks[i]->op_code.load(std::memory_order_relaxed)
            );
        }

        // Call user defined execute of operation
        executor_tasks.at(0)->user_op.template execute<SHOT_N>(user_tasks, n);

        for (size_t i = 0; i < n; i++) {
            executor_tasks.at(i)->op_code.store(Operation<OpNode>::NOT_SET, std::memory_order_relaxed);
            executor_tasks.at(i)->complete.store(true, std::memory_order_release);
        }

    }
};

template<typename OpNode, size_t SHOT_N>
FlatCombiner<OpNode, SHOT_N>::FlatCombiner():
    _lock(0),
    _slot(nullptr, std::function<void(void*)>(slot_destructor)) {

    auto dummy_slot_head = new Operation<OpNode>();
    _queue.store(dummy_slot_head, std::memory_order_relaxed);

    auto dummy_slot_tail = new Operation<OpNode>();
    _dummy_tail = dummy_slot_tail;
    FlatCombiner::push_to_queue(dummy_slot_tail);
}

template<typename OpNode, size_t SHOT_N>
FlatCombiner<OpNode, SHOT_N>::~FlatCombiner() {
    // All threads should do detach() before FlatCombiner destructor call (!)
    delete _dummy_tail;
    delete _queue;
}

template<typename OpNode, size_t SHOT_N>
Operation<OpNode> *FlatCombiner<OpNode, SHOT_N>::get_slot() {

    Operation<OpNode> *slot = _slot.get();

    if (slot == nullptr) {
        slot = new Operation<OpNode>();
        _slot.set(slot);
    }

    return slot;
}

} // namespace FlatCombiner

#endif //FLAT_COMBINE_FLATCOMBINER_H
