#include "Storage.h"
#include <algorithm>
#include <tuple>

//tmp
#include <iostream>
#include <sstream>

namespace Repository {

/* StorageSlot implementation */

void StorageSlot::prepare_data(const std::string &key, const std::string &value) {
    _key = key;
    _value = value;
}

void StorageSlot::prepare_data(const std::string &key) {
    std::string tmp;
    prepare_data(key, tmp);
}

std::string StorageSlot::get_data() {
    return std::move(_value);
}

// Comparator for optimize requests
bool StorageSlot::comparator(const task_type &a, const task_type &b) {

    // Compare keys for storage
    if (std::get<0>(a)->_key < std::get<0>(b)->_key)
        return true;

    if (std::get<0>(a)->_key > std::get<0>(b)->_key)
        return false;

    // If equals compare operation codes
    return std::get<1>(a) < std::get<1>(b);
}

void StorageSlot::init(std::shared_ptr<Storage> storage) {
    _storage = std::move(storage);
}

std::string &StorageSlot::get_key() {
    return _key;
}

std::string &StorageSlot::get_value() {
    return _value;
}

} // namespace Repository