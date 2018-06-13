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

    // Initialize slot after creation
    void init(std::shared_ptr<Storage> storage);

    // Execute operation in Slot
    void execute(int op_code);

    // Prepare request data before execute operation
    void prepare_data(const std::string &key, const std::string &value);
    void prepare_data(const std::string &key);

    // Get data from read operations
    std::string get_data();

    static bool comparator(const StorageSlot &a, const StorageSlot &b);
    static void optimize_queue(StorageSlot *begin, StorageSlot *end);
private:

    /* Instance of Storage */
    std::shared_ptr<Storage> _storage;

    /* Data for operation */
    std::string _key;
    std::string _value;
};
#endif //FLAT_COMBINE_STORAGE_H
