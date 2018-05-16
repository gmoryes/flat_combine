#include "Storage.h"
#include <algorithm>

/* Storage implementation */

bool Storage::Execute(Storage::Operation op, const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);

    switch (op) {
        case PUT:
            _storage[key] = value;
            return true;
        case GET:
        {
            auto it = _storage.find(key);
            if (it == _storage.end())
                return false;
            else
                value = it->second;
            return true;
        }
        case DELETE:
        {
            auto it = _storage.find(key);
            if (it == _storage.end())
                return false;
            else
                _storage.erase(it);
            return true;
        }
        default:
            throw std::runtime_error("Unsupported operation");
    }
}

bool Storage::Execute(Storage::Operation op, const std::string &key) {
    std::string tmp;
    return Execute(op, key, tmp);
}


/* StorageSlot implementation */

bool StorageSlot::complete() {
    return _complete.load(std::memory_order_acquire);
}

bool StorageSlot::error(std::exception &ex) {
    return _error.load(std::memory_order_acquire);
}

void StorageSlot::execute() {
    try {
        _storage->Execute(_op_code.load(std::memory_order_relaxed), _key, _value);
    } catch (std::exception &ex) {
        _error.store(true, std::memory_order_release);
        _exception = ex;
    }
    _complete.store(true, std::memory_order_release);
    _op_code.store(Storage::Operation::NOT_SET, std::memory_order_release);
}

void StorageSlot::set_operation(Storage::Operation op_code, const std::string &key, const std::string &value) {
    _key = key;
    _value = value;
    _complete.store(false, std::memory_order_relaxed);
    _error.store(false, std::memory_order_relaxed);
    _op_code.store(op_code, std::memory_order_release);
}

void StorageSlot::set_operation(Storage::Operation op_code, const std::string &key) {
    std::string tmp;
    set_operation(op_code, key, tmp);
}

std::string StorageSlot::get_data() {
    return _value;
}

bool StorageSlot::has_data() {
    //return _op_code.load(std::memory_order_acquire) != Storage::Operation::NOT_SET;
    return _op_code.load() != Storage::Operation::NOT_SET;
}

bool StorageSlot::comparator(const StorageSlot &a, const StorageSlot &b) {
    if (a._key < b._key)
        return true;

    if (a._key > b._key)
        return false;

    return a._op_code.load(std::memory_order_relaxed) < b._op_code.load(std::memory_order_relaxed);
}

void StorageSlot::optimize_queue(StorageSlot *begin, StorageSlot *end) {
    // TODO add operator=()
    // std::stable_sort(begin, end, StorageSlot::comparator);
}

