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

std::mutex mutex123;
void logg(std::string str) {
    std::lock_guard<std::mutex> lock(mutex123);
    str += "\n";
    std::cout << std::hex << str;
}

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
template <typename SharedStructure, typename OpNode>
class FlatCombiner {
public:
    // User defined type for the pending operations description, must be plain object without
    // virtual functions
    using pending_operation = OpNode;

    // Function that combine multiple operations and apply it onto data structure
    using combiner = std::function<void(OpNode *, OpNode *)>;

    // Extend user provided pending operation type with fields required for the
    // flat combine algorithm to work
    using Slot = struct Slot {
        static constexpr uint64_t THREAD_ALIVE_BIT = uint64_t(1) << 63L;
        static constexpr uint64_t CLEAR_THREAD_ALIVE_BIT = ~THREAD_ALIVE_BIT;

        Slot (std::shared_ptr<SharedStructure> storage):
            user_op(storage),
            generation(0),
            next_and_alive(0),
            alive_flag(true) {}

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
        // in the queue. If pointer is zero and bit is set then the only ref
        // left is thread_local storage. If next is zero there are no
        // link left and slot could be deleted
        std::atomic<Slot*> next_and_alive;
        std::atomic<bool> alive_flag;

        Slot *next() {
            return next_and_alive.load(std::memory_order_acquire);
        }

        /*
         * Return true if slot is still in queue, check that next_and_alive non zero
         */
        bool in_queue() const {
            return next_and_alive.load(std::memory_order_acquire) != nullptr;
        }

        bool is_alive() const {
            return alive_flag.load(std::memory_order_acquire);
        }
    };

    /**
     * @param Combine function that aplly pending operations onto some data structure. It accepts array
     * of pending ops and allowed to modify it in any way except delete pointers
     */ /* _slot(nullptr, orphan_slot) */
    FlatCombiner(combiner combine): _slot(nullptr/*, orphan_slot */), _combine(combine), _lock(0) {

        _data_structure = std::make_shared<SharedStructure>();
        auto dummy_slot_head = new Slot(_data_structure);
        _queue.store(dummy_slot_head, std::memory_order_relaxed);

        auto dummy_slot_tail = new Slot(_data_structure);
        _dummy_tail = dummy_slot_tail;
        FlatCombiner::push_to_queue(dummy_slot_tail);
    }
    ~FlatCombiner() { /* dequeue all slot, think about slot deletition */ }

    /**
     * Return pending operation slot to the calling thread, object stays valid as long
     * as current thread is alive or got called detach method
     */
    pending_operation *get_slot() {

        Slot *slot = _slot.get();

        if (slot == nullptr) {
            slot = new Slot(_data_structure);
            std::stringstream ss;
            ss << "recv slot(" << slot << ")"; logg(ss.str());
            _slot.set(slot);
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
        Slot *slot = _slot.get();
        if (slot == nullptr)
            throw std::runtime_error("Received nullptr");

        std::stringstream ss;
        ss << "check in queue slot(" << slot << ")"; logg(ss.str());
        if (not slot->in_queue())
            FlatCombiner::push_to_queue(slot);

        while (not slot->user_op.complete()) {
            if (uint64_t generation = FlatCombiner::try_lock(std::memory_order_release, std::memory_order_relaxed)) {
                // Yep!! We are executor
                std::stringstream ss;
                ss << "slot(" << slot << ") is executor"; logg(ss.str());

                FlatCombiner::run_executor(generation);
                FlatCombiner::unlock();

                std::stringstream ss1;
                ss1 << "slot(" << slot << ") unlock" << std::endl; logg(ss1.str());
                return;
            } else {
                // :((
                std::this_thread::yield();
            }
        }

        std::stringstream ss2;
        ss2 << "slot(" << slot << ") apply completed"; logg(ss2.str());
    }

    /**
     * Detach calling thread from this flat combiner, in other word
     * destroy thread slot in the queue
     */
    void detach() {
        Slot *slot = _slot.get();
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

        std::stringstream ss1;
        ss1 << "dequeue_slot(): slot(" << need_remove << "), parent(" << parent << ")"; logg(ss1.str());
        auto new_next = need_remove->next();

        std::stringstream ss2;
        ss2 << "dequeue_slot(): new_next(" << new_next << ")"; logg(ss2.str());

        auto copy_expected_value = need_remove;
        while (not parent->next_and_alive.compare_exchange_strong(
            need_remove,
            new_next,
            std::memory_order_release,
            std::memory_order_acquire)) {

            need_remove = copy_expected_value;
            parent = parent->next();
        }

        std::stringstream ss3;
        ss3 << "dequeue_slot(): cas done, parent is(" << parent << ")"; logg(ss3.str());
        need_remove->next_and_alive.store(nullptr, std::memory_order_release);
    }

    /**
     * Function called once thread owning this slot is going to die or to
     * destory slot in some other way
     *
     * @param slot modified adress to the slot is being to orphan
     */
    void orphan_slot(Slot *slot_to_delete) {

        if (not slot_to_delete->in_queue()) {
            // THe only reference to it was in ThreadLocal, can safely delete it
            std::stringstream ss5;
            ss5 << std::hex << "orphan_slot(" << slot_to_delete << ") delete"; logg(ss5.str());
            delete slot_to_delete;
            return;
        }

        std::stringstream ss5;
        ss5 << std::hex << "orphan_slot(" << slot_to_delete << ") mark as dead"; logg(ss5.str());
        // Say other threads, that we are dead, executor will delete pointer
        slot_to_delete->alive_flag.store(false, std::memory_order_release);
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
    std::atomic<Slot*> _queue{};
    Slot *_dummy_tail;

    // Insert between dummy slot and next to it
    void push_to_queue(Slot* new_node) {

        auto head = _queue.load(std::memory_order_relaxed);

        Slot *next;
        std::stringstream ss;
        ss << std::hex << "Before: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
        ss << ", current(" << new_node << ")"; logg(ss.str());

        do {
            next = head->next_and_alive.load(std::memory_order_acquire);
            new_node->next_and_alive.store(next, std::memory_order_relaxed);
        } while (!head->next_and_alive.compare_exchange_weak(
            next,
            new_node,
            std::memory_order_release,
            std::memory_order_acquire));

        std::stringstream ss1;
        ss1 << std::hex << "After: head(" << head << "), next(" << head->next_and_alive.load(std::memory_order_relaxed) << ")";
        ss1 << ", next(" << new_node->next_and_alive.load(std::memory_order_relaxed);
        ss1 << ")"; logg(ss1.str());

    }

    // Function to call in order to execute operations
    combiner _combine;

    // Usual strategy for the combine flat would be sort operations by some creteria
    // and optimize it somehow. That array is using by executor thread to prepare
    // number of ops to pass to combine
    //TODO return 64
    std::array<OpNode *, 64> _combine_shot;

    // Slot of the current thread. If nullptr then cur thread gets access in the
    // first time or after a long period when slot has been deleted already
    ThreadLocal<Slot> _slot;

    void run_executor(uint64_t generation) {

        auto parent = _queue.load(std::memory_order_relaxed);
        auto current_node = parent->next_and_alive.load(std::memory_order_relaxed);

        int n = 0;
        while (current_node != _dummy_tail) {
            std::stringstream ss;
            ss << "run_executor(" << generation << "): get current_node(" << std::hex << current_node << ")"; logg(ss.str());

            std::stringstream ss1;
            ss1 << std::hex << "check slot for alive(" << current_node << ")"; logg(ss1.str());
            if (current_node->is_alive()) {

                std::stringstream ss1;
                ss1 << std::hex << "slot alive_ok(" << current_node << ")"; logg(ss1.str());
                if (current_node->user_op.has_data()) {

                    std::stringstream ss;
                    ss << "slot(" << current_node << ") has data"; logg(ss.str());
                    current_node->generation = generation;
                    _combine_shot[n] = &current_node->user_op;
                    n++;
                    parent = current_node;

                } else if (generation - current_node->generation > MAX_AWAIT_TIME) {

                    std::stringstream ss;
                    ss << "gen of slot(" << current_node << ") = " << current_node->generation;
                    ss << " current = " << generation; logg(ss.str());
                    FlatCombiner::dequeue_slot(parent, current_node);

                    // For call next_and_alive from parent, slot->next_and_alive is zero now
                    current_node = parent;
                } else {
                    parent = current_node;
                }

            } else {
                std::stringstream ss1;
                ss1 << std::hex << "slot alive_bad(" << current_node << ")"; logg(ss1.str());
                FlatCombiner::dequeue_slot(parent, current_node);

                // Only owner of this ThreadLocal variable could unset the alive flag, if it zero
                // the only reference to it in parent, so we safely delete after change Next of parent
                std::stringstream ss;
                ss << std::hex << "delete(" << current_node << ")_2"; logg(ss.str());
                delete current_node;

                // For call next_and_alive from parent, slot->next_and_alive is zero now
                current_node = parent;
            }

            current_node = current_node->next_and_alive.load(std::memory_order_acquire);
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
