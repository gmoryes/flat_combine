#ifndef FLAT_COMBINE_STORAGE_H
#define FLAT_COMBINE_STORAGE_H

#include <mutex>
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <utility>
#include <array>
#include <algorithm>

namespace Repository {

class Storage;
class StorageSlot;

// 1. Pointer to StorageSlot
// 2. Operation code (pass to storage.Execute())
using task_type = std::pair<StorageSlot*, int>;

class StorageSlot {
public:

    /* Initialize slot after creation */
    void init(std::shared_ptr<Storage> storage);

    /* Prepare request data before execute operation */
    void prepare_data(const std::string &key, const std::string &value);
    void prepare_data(const std::string &key);

    /* Get data from read operations */
    std::string get_data();

    /* Get key from slot */
    std::string &get_key();

    /* Get value from slot */
    std::string &get_value();

    /**
     * Execute operations, this array will be passed to Storage
     * @tparam SHOT_N - amount of max number of tasks
     * @param tasks - array of tasks
     * @param n - amount of tasks in array now
     */
    template<std::size_t SHOT_N>
    void execute(std::array<task_type, SHOT_N> &tasks, size_t n);

    /* Comparator for std::sort */
    static bool comparator(const task_type &a, const task_type &b);

    /* Error code, set by Storage */
    int error_code;
private:

    /* Instance of Storage */
    std::shared_ptr<Storage> _storage;

    /* Data for operation */
    std::string _key;
    std::string _value;
};

// Simple storage
class Storage {
public:

    Storage() = default;
    ~Storage() = default;

    // Possible operation
    enum Operation {
        NOT_SET,
        PUT,
        GET,
        DELETE
    };

    enum ErrorCode {
        OK,
        NOT_FOUND,
        UNSUPPORTED_OPERATION
    };

    /**
     * Execute pack of tasks
     * @tparam SHOT_N - number of max tasks number in array
     * @param tasks - array of tasks
     * @param n - amount of tasks in array now
     */
    template<std::size_t SHOT_N>
    void Execute(std::array<task_type, SHOT_N> &tasks, size_t n) {

        std::lock_guard<std::mutex> lock(_mutex);
        int op_code;
        StorageSlot *slot;
        auto hint = _storage.begin();

        for (size_t i = 0; i < n; i++) {

            std::tie(slot, op_code) = tasks.at(i);
            std::string &key = slot->get_key();
            std::string &value = slot->get_value();

            switch (op_code) {
                case PUT: {
                    //hint = _storage.emplace_hint(hint, key, value);
                    _storage[key] = value;
                    std::get<0>(tasks.at(i))->error_code = ErrorCode::OK;
                    break;
                }
                case GET: {
                    auto it = _storage.find(key);
                    if (it == _storage.end()) {
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::NOT_FOUND;
                    } else {
                        //hint = it;
                        value = it->second;
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::OK;
                    }
                    break;
                }
                case DELETE: {
                    auto it = _storage.find(key);
                    if (it == _storage.end()) {
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::NOT_FOUND;
                    } else {
                        //hint = it;
                        _storage.erase(it);
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::OK;
                    }
                    break;
                }
                default:
                    std::get<0>(tasks.at(i))->error_code = ErrorCode::UNSUPPORTED_OPERATION;
            }

        }
    }

private:
    std::mutex _mutex;
    std::map<std::string, std::string> _storage;
};

template <std::size_t SHOT_N>
void StorageSlot::execute(std::array<task_type, SHOT_N> &tasks, size_t n) {

    // Sort keys for optimize queries with use std::map<>::emplace_hint()
    std::stable_sort(tasks.begin(), tasks.begin() + n, StorageSlot::comparator);

    _storage->Execute<SHOT_N>(tasks, n);
}

} // namespace Repository

#endif //FLAT_COMBINE_STORAGE_H
