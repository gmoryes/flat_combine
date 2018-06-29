#ifndef FLAT_COMBINE_TAGGEDPOINTER_H
#define FLAT_COMBINE_TAGGEDPOINTER_H

#include <atomic>

namespace FlatCombiner {

/**
 * Tagged pointer
 * @tparam T - type of Pointer
 */
template<typename T>
class TaggedPointer {
public:

    /* Bit mask of tag */
    static constexpr uintptr_t ALIVE_BIT = 1L << 63;

    explicit TaggedPointer(T *p) {
        _ptr.store(reinterpret_cast<uintptr_t>(p) | ALIVE_BIT);
    }

    /**
     * Return pointer without tag
     * @param read
     * @return
     */
    T *get(std::memory_order read) const {
        return reinterpret_cast<T *>(_ptr.load(read) & ~ALIVE_BIT);
    }

    /**
     * Set pointer with tag saving
     * @param value_ - new value
     * @param read - read memory order
     * @param write - write memory order
     */
    void set(T *value_,
             std::memory_order read = std::memory_order_relaxed,
             std::memory_order write = std::memory_order_relaxed) {

        auto old = _ptr.load(read);
        uintptr_t mask = old & ALIVE_BIT;
        auto value = reinterpret_cast<uintptr_t>(value_);
        _ptr.store(value | mask, write);
    }

    /**
     * Unset tag
     * @param read - read memory order
     * @param write - write memory order
     */
    void unset_flag(std::memory_order read, std::memory_order write) {
        auto old = _ptr.load(read);
        old &= ~ALIVE_BIT;
        _ptr.store(old, write);
    }

    /**
     * Check if tag is set
     * @param read - read memory order
     * @return true if set, false otherwise
     */
    bool is_flag_set(std::memory_order read) const {
        return (_ptr.load(read) & ALIVE_BIT) != 0;
    }

    /**
     * Represent compare_exchange_weak on atomic variable with saving tag.
     * Update expected_ value if CAS failed
     * @param expected_ - expected value
     * @param new_value_ - new value
     * @param suc - memory order in case of success CAS
     * @param fail - memory order in case of failed CAS
     * @return true if CAS success and false otherwise
     */
    bool compare_exchange_weak(T *&expected_, T *new_value_,
                               std::memory_order suc, std::memory_order fail) {

        auto flag = _ptr.load(std::memory_order_relaxed) & ALIVE_BIT;

        auto expected = reinterpret_cast<uintptr_t>(expected_) | flag;
        auto new_value = reinterpret_cast<uintptr_t>(new_value_) | flag;

        bool result = _ptr.compare_exchange_weak(expected, new_value, suc, fail);
        if (result)
            return result;

        // Update new value, if CAS failed
        expected &= ~ALIVE_BIT;
        expected_ = reinterpret_cast<T *>(expected);

        return false;
    }

    /**
     * Close to method - compare_exchange_weak. But not update expected_ value
     * because that doesn't need in algorithm.
     */
    bool compare_exchange_strong(T *expected_, T *new_value_,
                                 std::memory_order suc, std::memory_order fail) {

        auto flag = _ptr.load(std::memory_order_relaxed) & ALIVE_BIT;

        auto expected = reinterpret_cast<uintptr_t>(expected_) | flag;
        auto new_value = reinterpret_cast<uintptr_t>(new_value_) | flag;

        return _ptr.compare_exchange_strong(expected, new_value, suc, fail);
    }

    /**
     * Update pointer to value_
     * @param value_ - new value
     * @param flag - expected flag
     * @return true if ((flag is true and tag is set) or (flag is false and tag is not set)), false otherwise
     */
    bool update_value_with_tag_check(T *value_, bool flag) {
        bool result = true;
        uintptr_t mask = flag ? ALIVE_BIT : 0;

        auto expected = _ptr.load(std::memory_order_relaxed);
        expected = ~ALIVE_BIT;
        expected |= mask;

        auto value = reinterpret_cast<uintptr_t>(value_);
        do {
            value &= ~ALIVE_BIT;
            value |= (expected & ALIVE_BIT);

            result = (mask == (expected & ALIVE_BIT));
        } while (!_ptr.compare_exchange_weak(expected, value, std::memory_order_release, std::memory_order_relaxed));

        return result;
    }

private:
    std::atomic<uintptr_t> _ptr;
};

} // namespace FlatCombiner
#endif //FLAT_COMBINE_TAGGEDPOINTER_H
