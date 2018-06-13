#include "Storage.h"
#include <algorithm>
#include "Logger/Logger.h"

//tmp
#include <iostream>
#include <sstream>

/* Storage implementation */

bool Storage::Execute(Storage::Operation op, const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);

    std::stringstream ss;
    ss << "start execute(" << op <<"), key(" << key << "), value(" << value << ")"; my_log(ss);
    switch (op) {
        case PUT:
            _storage[key] = value;
            return true;
        case GET:
        {
            auto it = _storage.find(key);
            if (it == _storage.end()) {
                std::stringstream ss;
                ss << "execute(): get not found for key(" << key << ")"; my_log(ss);
                throw std::runtime_error("key not found");
                return false;
            } else {
                std::stringstream ss;
                ss << "execute(): get for key(" << key << "), get_value(" << it->second << ")"; my_log(ss);
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

void StorageSlot::execute(int op_code) {
    //try {
        _storage->Execute(static_cast<Storage::Operation>(op_code), _key, _value);
    //} catch (std::exception &ex) {
        //_error.store(true, std::memory_order_release);
        //_exception = ex;
    //}
    std::stringstream ss1;
    ss1 << "execute(): after Execute(), key(" << _key << "), value(" << _value << ")"; my_log(ss1);

    std::stringstream ss;
    ss << "execute(): END, set _op_code(0) for key(" << _key << ")"; my_log(ss);
    //_op_code.store(Storage::Operation::NOT_SET, std::memory_order_release);
    //_complete.store(true, std::memory_order_release);
}

void StorageSlot::prepare_data(const std::string &key, const std::string &value) {
    _key = key;
    _value = value;
}

void StorageSlot::prepare_data(const std::string &key) {
    std::string tmp;
    prepare_data(key, tmp);
}

std::string StorageSlot::get_data() {
    std::stringstream ss;
    ss << "get_data(): key(" << _key << "), " << "_value(" << _value << ")"; my_log(ss);
    return std::move(_value);
}

// TODO fix this function
bool StorageSlot::comparator(const StorageSlot &a, const StorageSlot &b) {
    if (a._key < b._key)
        return true;

    if (a._key > b._key)
        return false;

    return true; // TODO fix it
    //return a._op_code.load(std::memory_order_relaxed) < b._op_code.load(std::memory_order_relaxed);
}

void StorageSlot::optimize_queue(StorageSlot *begin, StorageSlot *end) {
    // TODO add operator=()
    // std::stable_sort(begin, end, StorageSlot::comparator);
}

void StorageSlot::init(std::shared_ptr<Storage> storage) {
    _storage = std::move(storage);
}

/*

 slot = flat_combine->get_slot();
 slot->init(...); // UD

 slot->set_operation(op_code, args...) // FCD
 void set_operation(int op_code, args...) {
    _op_code = op_code;
    this->init_before_query(args...);
 }

 flat_combine->apply_slot();






*/
