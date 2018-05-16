#include "gtest/gtest.h"
#include "storage/Storage.h"
#include "FlatCombiner/FlatCombiner.h"
#include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <iostream>

using shared_combiner_t = std::shared_ptr<FlatCombiner<Storage, StorageSlot>>;

bool check_error(StorageSlot *storage_slot) {
    std::exception ex;
    if (storage_slot->error(ex)) {
        std::cout << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}

const int MAX_OPERATION_PER_THREAD = 2;
void worker(int number, const shared_combiner_t &flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    std::cout << number << " get slot" << std::endl;
    StorageSlot *storage_slot = flat_combiner->get_slot();

    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string key = ss.str() + "_key_";
    std::string value = ss.str();

    std::vector<std::string> keys(MAX_OPERATION_PER_THREAD);
    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss_full(key);
        ss_full << rand();
        keys[i] = ss_full.str();
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::cout << number << " set put" << std::endl;
        storage_slot->set_operation(Storage::Operation::PUT, keys[i], value);
        std::cout << number << " apply put" << std::endl;
        flat_combiner->apply_slot();
        check_error(storage_slot);
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::cout << number << " set get" << std::endl;
        storage_slot->set_operation(Storage::Operation::GET, keys[i]);
        std::cout << number << " apply get" << std::endl;
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::string result = storage_slot->get_data();
        EXPECT_TRUE(result == value);
    }
}

TEST(FlatCombineLogicTest, PutTest) {
    std::cout << "pid(" << getpid() << ")" << std::endl;
    Storage storage;
    auto shared_flat_combiner = std::make_shared<FlatCombiner<Storage, StorageSlot>>(StorageSlot::optimize_queue);

    std::vector<std::thread> workers;
    int workers_number = 2;
    for (int i = 0; i < workers_number; i++) {
        workers.emplace_back(&worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}