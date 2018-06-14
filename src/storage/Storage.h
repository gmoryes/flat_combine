#ifndef FLAT_COMBINE_STORAGE_H
#define FLAT_COMBINE_STORAGE_H

#include <mutex>
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <utility>
#include <array>
#include "Logger/Logger.h"
#include <algorithm>

namespace Repository {

class Storage;
class StorageSlot;

// 1. Pointer to StorageSlot
// 2. Operation code (pass to storage.Execute())
using task_type = std::pair<StorageSlot*, int>;

class StorageSlot {
public:

    // Initialize slot after creation
    void init(std::shared_ptr<Storage> storage);

    // Prepare request data before execute operation
    void prepare_data(const std::string &key, const std::string &value);

    void prepare_data(const std::string &key);

    // Get data from read operations
    std::string get_data();

    std::string &get_key() {
        return _key;
    }

    std::string &get_value() {
        return _value;
    }

    template<std::size_t SHOT_N>
    void execute(std::array<task_type, SHOT_N> &tasks, size_t n);

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

// Stupid storage
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

    template<std::size_t SHOT_N>
    void Execute(std::array<task_type, SHOT_N> &tasks, size_t n) {

        std::lock_guard<std::mutex> lock(_mutex);
        int op_code;
        StorageSlot *slot;

        for (size_t i = 0; i < n; i++) {

            std::tie(slot, op_code) = tasks.at(i);
            std::string &key = slot->get_key();
            std::string &value = slot->get_value();

//            std::stringstream ss;
//            ss << "Execute(): start(" << op_code << "), key(" << key << "), value(" << value << ")";
//            my_log(ss);
            switch (op_code) {
                case PUT: {
                    _storage[key] = value;
                    std::get<0>(tasks.at(i))->error_code = ErrorCode::OK;
                    break;
                }
                case GET: {
                    auto it = _storage.find(key);
                    if (it == _storage.end()) {
//                        std::stringstream ss;
//                        ss << "execute(): get not found for key(" << key << ")";
//                        my_log(ss);
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::NOT_FOUND;
                    } else {
//                        std::stringstream ss;
//                        ss << "execute(): get for key(" << key << "), get_value(" << it->second << ")";
//                        my_log(ss);
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
                        _storage.erase(it);
                        std::get<0>(tasks.at(i))->error_code = ErrorCode::OK;
                    }
                    break;
                }
                default:
                    std::get<0>(tasks.at(i))->error_code = ErrorCode::UNSUPPORTED_OPERATION;
            }

//            std::stringstream ss1;
//            ss1 << "Execute(): end execute(" << op_code << "), key(" << key << "), value("
//                << value << "), error(" << std::get<0>(tasks.at(i))->error_code << ")";
//            my_log(ss1);
        }
    }

private:
    std::mutex _mutex;
    std::map<std::string, std::string> _storage;
};

template <std::size_t SHOT_N>
void StorageSlot::execute(std::array<task_type, SHOT_N> &tasks, size_t n) {

    std::stable_sort(tasks.begin(), tasks.begin() + n, StorageSlot::comparator);

//    std::stringstream ss1;
//    ss1 << "execute(): before(";
//    for (int i = 0; i < n; i++) {
//        ss1 << std::get<2>(tasks[i]) << " ";
//    }
//    ss1 << ")";
//    my_log(ss1);

    _storage->Execute<SHOT_N>(tasks, n);

//    std::sort(
//        result.begin(),
//        result.begin() + n,
//        [&tasks](const respone_type_with_id &a, const respone_type_with_id &b) {
//            return std::get<2>(tasks[std::get<1>(a)]) < std::get<2>(tasks[std::get<1>(b)]);
//        }
//    );

//    for (size_t i = 0; i < n ; i++) {
//        result_.at(i) = std::get<0>(result.at(i));
//    }
//
//    std::stringstream ss2;
//    ss2 << "execute(): after(";
//    for (int i = 0; i < n; i++) {
//        ss2 << std::get<2>(tasks[i]) << " ";
//    }
//    ss2 << ")";
//    my_log(ss2);
}

} // namespace Repository

#endif //FLAT_COMBINE_STORAGE_H
