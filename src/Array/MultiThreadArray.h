#ifndef FLAT_COMBINE_MULTITHREADARRAY_H
#define FLAT_COMBINE_MULTITHREADARRAY_H

#include <array>
#include <memory>

namespace Array {

template <typename Type, std::size_t SIZE>
class MultiThreadArraySlot;

template <typename Type, std::size_t SIZE>
class MultiThreadArray {
public:

    using task_type = std::pair<MultiThreadArraySlot<Type, SIZE>*, int>;

    /*
     * Operation codes, this data stored in FlatCombine, and passed to shared structure
     * and used in Execute() method
     */
    enum Operation {
        NOT_SET, // <==== always should be (!)
        SET,
        GET,
        SET_TO_ZERO
    };

    /*
     * Error codes, this data stored in MultiThreadArraySlot (int error_code), and user
     * can check result of execution (is it OK or the error has happend)
     */
    enum ErrorCode {
        OK,
        BAD_INDEX,
        UNSUPPORTED_OPERATION
    };

    /*
     * This is user's method of execution pack of task, could be implemented as user want
     */
    template <std::size_t SHOT_N>
    void Execute(std::array<task_type, SHOT_N> &tasks, size_t n) {
        MultiThreadArraySlot<Type, SIZE> *slot;
        int op_code;

        for (size_t i = 0; i < n; i++) {
            // Receive request info (slot and operation code)
            std::tie(slot, op_code) = tasks[i];

            // Set error_code to some value if error happend
            int index = slot->index();
            if (not check_index(index)) {
                slot->error_code = ErrorCode::BAD_INDEX;
                continue;
            }

            // Set error_code to OK if all good and to UNSUPPORTED_OPERATION
            // if we don't know such operation code
            switch (op_code) {
                case Operation::SET:
                    _array[index] = slot->data();
                    slot->error_code = ErrorCode::OK;
                    break;

                case Operation::GET:
                    slot->data(_array[index]);
                    slot->error_code = ErrorCode::OK;
                    break;

                case Operation::SET_TO_ZERO:
                    _array[index] = 0;
                    slot->error_code = ErrorCode::OK;
                    break;

                default: {

                    slot->error_code = ErrorCode::UNSUPPORTED_OPERATION;
                    break;
                }
            }
        }
    }
private:

    bool check_index(int index) {
        return 0 <= index && index < SIZE;
    }

    std::array<Type, SIZE> _array;
};

template <typename Type, std::size_t SIZE>
class MultiThreadArraySlot {
public:

    using task_type = std::pair<MultiThreadArraySlot<Type, SIZE>*, int>;

    /* Need for flat combiner */
    template <std::size_t SHOT_N>
    void execute(std::array<task_type, SHOT_N> &tasks, size_t n) {
        _storage->Execute(tasks, n);
    }

    void prepare_data(int index) {
        _index = index;
    }

    void prepare_data(int index, const Type &value) {
       prepare_data(index);
        _data = value;
    }



    int error_code;

    /* Some functions not related to FlatCombine */
    int index() const {
        return _index;
    }

    Type &data() {
        return _data;
    }

    void data(Type &new_data) {
        _data = new_data;
    }

    /*
     * Note, that after do flat_combine->get_slot(), we received empty slot
     * (we don't have any reference to our common structure, so use want to initialize it.
     *
     * This could do FlatCombiner, but in this case it will overload different user data
     * types and functions.
     */
    void init(const std::shared_ptr<MultiThreadArray<Type, SIZE>> &storage) {
        _storage = std::move(storage);
    }
private:

    /* Some user data for execute query */
    std::shared_ptr<MultiThreadArray<Type, SIZE>> _storage;
    Type _data;
    int _index;
};

}; // namespace Array

#endif //FLAT_COMBINE_MULTITHREADARRAY_H
