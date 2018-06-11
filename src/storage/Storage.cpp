#include "Storage.h"
#include <algorithm>

//tmp
#include <iostream>
#include <sstream>

/* Storage implementation */

std::mutex mutex3211;
void logggg(std::string str) {
    std::lock_guard<std::mutex> lock(mutex3211);
    str += "\n";
    std::cout << std::hex << str;
}

bool Storage::Execute(Storage::Operation op, const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);

    std::stringstream ss;
    ss << "start execute(" << op <<"), key(" << key << "), value(" << value << ")"; logggg(ss.str());
    switch (op) {
        case PUT:
            _storage[key] = value;
            return true;
        case GET:
        {
            auto it = _storage.find(key);
            if (it == _storage.end()) {
                std::stringstream ss;
                ss << "execute(): get not found for key(" << key << ")"; logggg(ss.str());
                throw std::runtime_error("key not found");
                return false;
            } else {
                std::stringstream ss;
                ss << "execute(): get for key(" << key << "), get_value(" << it->second << ")"; logggg(ss.str());
                value = it->second;
            }
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
    std::stringstream ss;
    ss << "execute(): END, set _op_code(0) for key(" << _key << ")"; logggg(ss.str());
    _op_code.store(Storage::Operation::NOT_SET, std::memory_order_release);
    _complete.store(true, std::memory_order_release);
}

void StorageSlot::set_operation(Storage::Operation op_code, const std::string &key, const std::string &value) {
    _key = key;
    _value = value;
    _complete.store(false, std::memory_order_relaxed);
    //_complete.store(false);
    _error.store(false, std::memory_order_relaxed);
    _op_code.store(op_code, std::memory_order_release);
    //_op_code.store(op_code);
    std::stringstream ss;
    ss << "set_operation(): key(" << key << "), op_code(" << op_code << ")"; logggg(ss.str());
}

void StorageSlot::set_operation(Storage::Operation op_code, const std::string &key) {
    std::string tmp;
    set_operation(op_code, key, tmp);
}

std::string StorageSlot::get_data() {
    std::stringstream ss;
    ss << "get_data(): key(" << _key << "), _op_code(" << _op_code << "), " << "_value(" << _value << ")"; logggg(ss.str());
    return std::move(_value);
}

bool StorageSlot::has_data() {
    //return _op_code.load(std::memory_order_acquire) != Storage::Operation::NOT_SET;
    std::stringstream ss;
    ss << "has_data(): key(" << _key << "), _op_code(" << _op_code.load() << ")"; logggg(ss.str());
    return _op_code.load(std::memory_order_acquire) != Storage::Operation::NOT_SET;
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

