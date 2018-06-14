#ifndef FLAT_COMBINE_GLOBALLOCKSTORAGE_H
#define FLAT_COMBINE_GLOBALLOCKSTORAGE_H

#include <mutex>
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <utility>

namespace GlobalLockStorage {

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

    // Possible Error Codes
    enum ErrorCode {
        OK,
        NOT_FOUND,
        UNSUPPORTED_OPERATION
    };

    int Execute(int op_code, const std::string &key, std::string &value);
    int Execute(int op_code, const std::string &key);

private:
    std::mutex _mutex;
    std::map<std::string, std::string> _storage;
};

} // GlobalLockStorage;

#endif //FLAT_COMBINE_GLOBALLOCKSTORAGE_H
