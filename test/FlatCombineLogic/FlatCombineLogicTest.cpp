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
#include <cstdlib>

std::mutex mutex321;
void loggg(std::string str) {
    std::lock_guard<std::mutex> lock(mutex321);
    std::cout << std::hex << str << std::endl;
}

using shared_combiner_t = std::shared_ptr<FlatCombiner<Storage, StorageSlot>>;

bool check_error(StorageSlot *storage_slot, bool must_be = false) {
    std::exception ex;
    if (storage_slot->error(ex)) {
        std::cout << "Error: " << ex.what() << std::endl;
        EXPECT_TRUE(must_be);
        return true;
    }

    return false;
}

const int MAX_OPERATION_PER_THREAD = 50000;
void put_get_worker(int number, std::shared_ptr<FlatCombiner<Storage, StorageSlot>> flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    std::stringstream ss1;
    ss1 << number << " get slot"; loggg(ss1.str());
    StorageSlot *storage_slot = flat_combiner->get_slot();

    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string key = ss.str() + "_key_";
    std::string value = ss.str();

    std::vector<std::string> keys(MAX_OPERATION_PER_THREAD);
    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss_full;
        ss_full << key << rand();
        keys[i] = ss_full.str();
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss2;
        ss2 << number << " set put(" << keys[i] << ")"; loggg(ss2.str());
        storage_slot->set_operation(Storage::Operation::PUT, keys[i], value);
        std::stringstream ss3;
        ss3 << number << " apply put(" << keys[i] << ")"; loggg(ss3.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss2;
        ss2 << number << " set get(" << keys[i] << ")"; loggg(ss2.str());
        storage_slot->set_operation(Storage::Operation::GET, keys[i]);
        std::stringstream ss3;
        ss3 << number << " apply get(" << keys[i] << ")"; loggg(ss3.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::string result = storage_slot->get_data();
        std::stringstream ss4;
        ss4 << number << " result(" << result << ")"; loggg(ss4.str());
        EXPECT_TRUE(result == value);
        if (result != value) {
            std::stringstream ss5;
            ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; loggg(ss5.str());

            abort();
        }
    }
}

void put_get_delete_worker(int number, const shared_combiner_t &flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    std::stringstream ss1;
    ss1 << number << " get slot"; loggg(ss1.str());
    StorageSlot *storage_slot = flat_combiner->get_slot();

    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string key = ss.str() + "_key_";
    std::string value = ss.str();

    std::vector<std::string> keys(MAX_OPERATION_PER_THREAD);
    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss_full;
        ss_full << key << rand();
        keys[i] = ss_full.str();
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        std::stringstream ss2;
        ss2 << number << " set put(" << keys[i] << ")"; loggg(ss2.str());
        storage_slot->set_operation(Storage::Operation::PUT, keys[i], value);
        std::stringstream ss3;
        ss3 << number << " apply put(" << keys[i] << ")"; loggg(ss3.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::stringstream ss4;
        ss4 << number << " set get(" << keys[i] << ")"; loggg(ss4.str());
        storage_slot->set_operation(Storage::Operation::GET, keys[i], value);
        std::stringstream ss6;
        ss6 << number << " apply get(" << keys[i] << ")"; loggg(ss6.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::string result = storage_slot->get_data();
        EXPECT_TRUE(result == value);
        if (result != value) {
            std::stringstream ss5;
            ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; loggg(ss5.str());

            abort();
        }

        std::stringstream ss7;
        ss7 << number << " set DELETE(" << keys[i] << ")"; loggg(ss7.str());
        storage_slot->set_operation(Storage::Operation::DELETE, keys[i], value);
        std::stringstream ss8;
        ss8 << number << " apply DELETE(" << keys[i] << ")"; loggg(ss8.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::stringstream ss9;
        ss9 << number << " set get(" << keys[i] << ")"; loggg(ss9.str());
        storage_slot->set_operation(Storage::Operation::GET, keys[i], value);
        std::stringstream ss10;
        ss10 << number << " apply get(" << keys[i] << ")"; loggg(ss10.str());
        flat_combiner->apply_slot();
        bool not_found = check_error(storage_slot, true);
        result = storage_slot->get_data();
        //EXPECT_TRUE(result == "");
        if (!not_found) {
            std::stringstream ss5;
            ss5 << number << " Fail, result(" << result << ")"; loggg(ss5.str());

            abort();
        }
    }

}

TEST(FlatCombineLogicTest, PutGetTest) {
    std::cout << "pid(" << getpid() << ")" << std::endl;
    Storage storage;
    auto shared_flat_combiner = std::make_shared<FlatCombiner<Storage, StorageSlot>>(StorageSlot::optimize_queue);

    std::vector<std::thread> workers;
    int workers_number = 20;
    for (int i = 0; i < workers_number; i++) {
        workers.emplace_back(&put_get_worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}


TEST(FlatCombineLogicTest, PutGetDeleteTest) {
    std::cout << "pid(" << getpid() << ")" << std::endl;
    Storage storage;

    std::function<void(StorageSlot*, StorageSlot*)> optimize_func(StorageSlot::optimize_queue);

    auto shared_flat_combiner = std::make_shared<FlatCombiner<Storage, StorageSlot>>(optimize_func);

    std::vector<std::thread> workers;
    int workers_number = 20;
    for (int i = 0; i < workers_number; i++) {
        workers.emplace_back(&put_get_delete_worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}