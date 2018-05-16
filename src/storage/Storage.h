#ifndef FLAT_COMBINE_STORAGE_H
#define FLAT_COMBINE_STORAGE_H

#include <mutex>
#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <utility>

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

    bool Execute(Operation op, const std::string &key, std::string &value);
    bool Execute(Operation op, const std::string &key);
private:
    std::mutex _mutex;
    std::map<std::string, std::string> _storage;
};

class StorageSlot {
public:

    explicit StorageSlot(std::shared_ptr<Storage> storage):
        _complete(false),
        _op_code(Storage::Operation::NOT_SET),
        _error(false),
        _storage(std::move(storage)) {}
    ~StorageSlot() = default;

    // Check if slot has data for execute
    bool has_data();

    // Check if operation is completed
    bool complete();

    // Check if error happened
    bool error(std::exception &ex);

    // Execute operation in Slot
    void execute();

    // Set operation to slot
    void set_operation(Storage::Operation op_code, const std::string &key, const std::string &value);
    void set_operation(Storage::Operation op_code, const std::string &key);

    // Get data from read operations
    std::string get_data();

    static bool comparator(const StorageSlot &a, const StorageSlot &b);
    static void optimize_queue(StorageSlot *begin, StorageSlot *end);
private:

    /* Instance of Storage */
    std::shared_ptr<Storage> _storage;

    /* Flag if operation has been completed */
    std::atomic<bool> _complete;

    /* Data for operation */
    std::string _key;
    std::string _value;

    /* Exeption */
    std::exception _exception;
    std::atomic<bool> _error;

    std::atomic<Storage::Operation> _op_code;
};
#endif //FLAT_COMBINE_STORAGE_H
