#include "GlobalLockStorage.h"

#include <algorithm>

namespace GlobalLockStorage {

/* GlobalLockStorage implementation */

int Storage::Execute(int op_code, const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);

    switch (op_code) {
        case PUT: {
            _storage[key] = value;
            return ErrorCode::OK;
        }
        case GET: {
            auto it = _storage.find(key);
            if (it == _storage.end()) {
                return ErrorCode::NOT_FOUND;
            } else {
                value = it->second;
                return ErrorCode::OK;
            }
        }
        case DELETE: {
            auto it = _storage.find(key);
            if (it == _storage.end())
                return ErrorCode::NOT_FOUND;
            else
                _storage.erase(it);
            return ErrorCode::OK;
        }
        default:
            return ErrorCode::UNSUPPORTED_OPERATION;
    }
}

int Storage::Execute(int op_code, const std::string &key) {
    std::string tmp;
    return Execute(op_code, key, tmp);
}

}; // GlobalLockStorage

