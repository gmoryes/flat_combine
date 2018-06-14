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
#include "Logger/Logger.h"

using namespace Repository;
using shared_combiner_t = std::shared_ptr<FlatCombiner::FlatCombiner<StorageSlot>>;
using shared_storage_t = std::shared_ptr<Storage>;
shared_storage_t shared_storage;

const int MAX_OPERATION_PER_THREAD = 100000;
const int workers_number = 10;

bool check_error(FlatCombiner::Operation<StorageSlot> *operation, bool must_be = false) {

    //std::stringstream ss;
    //ss << "ErrorCode(" << operation << "): " << operation->error_code() << ", must_error(" << must_be << ")"; my_log(ss);

    if (operation->error_code()) {

        EXPECT_TRUE(must_be);
        return true;
    }

    return false;
}

void put_get_worker(int number, shared_combiner_t flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    //std::stringstream ss1;
    FlatCombiner::Operation<StorageSlot> *operation = flat_combiner->get_slot();
    //ss1 << number << " get slot(" << operation << ")"; my_log(ss1);
    operation->user_slot()->init(shared_storage);

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
        //std::stringstream ss2;
        //ss2 << number << " set put(" << keys[i] << ")"; my_log(ss2);
        operation->set(Storage::Operation::PUT, keys[i], value);
        //std::stringstream ss3;
        //ss3 << number << " apply put(" << keys[i] << ")"; my_log(ss3);
        flat_combiner->apply_slot();
        check_error(operation);
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        //std::stringstream ss2;
        //ss2 << number << " set get(" << keys[i] << ")"; my_log(ss2);
        operation->set(Storage::Operation::GET, keys[i]);
        //std::stringstream ss3;
        //ss3 << number << " apply get(" << keys[i] << ")"; my_log(ss3);
        flat_combiner->apply_slot();
        check_error(operation);

        std::string result = operation->user_slot()->get_data();
        //std::stringstream ss4;
        //ss4 << number << " result(" << result << ")"; my_log(ss4);
        EXPECT_TRUE(result == value);
        if (result != value) {
            //std::stringstream ss5;
            //ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; my_log(ss5);

            abort();
        }
    }
}

void put_get_delete_worker(int number, shared_combiner_t flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    //std::stringstream ss1;
    FlatCombiner::Operation<StorageSlot> *operation = flat_combiner->get_slot();
    //ss1 << number << " get slot(" << operation << ")"; my_log(ss1);
    operation->user_slot()->init(shared_storage);

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
        //std::stringstream ss2;
        //ss2 << number << " set put(" << keys[i] << ")"; my_log(ss2);
        operation->set(Storage::Operation::PUT, keys[i], value);
        //std::stringstream ss3;
        //ss3 << number << " apply put(" << keys[i] << ")"; my_log(ss3);
        flat_combiner->apply_slot();
        check_error(operation);

        //std::stringstream ss4;
        //ss4 << number << " set get(" << keys[i] << ")"; my_log(ss4);
        operation->set(Storage::Operation::GET, keys[i], value);
        //std::stringstream ss6;
        //ss6 << number << " apply get(" << keys[i] << ")"; my_log(ss6);
        flat_combiner->apply_slot();
        check_error(operation);

        std::string result = operation->user_slot()->get_data();
        EXPECT_TRUE(result == value);
        if (result != value) {
            //std::stringstream ss5;
            //ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; my_log(ss5);

            abort();
        }

        //std::stringstream ss7;
        //ss7 << number << " set DELETE(" << keys[i] << ")"; my_log(ss7);
        operation->set(Storage::Operation::DELETE, keys[i], value);
        //std::stringstream ss8;
        //ss8 << number << " apply DELETE(" << keys[i] << ")"; my_log(ss8);
        flat_combiner->apply_slot();
        check_error(operation);

        //std::stringstream ss9;
        //ss9 << number << " set get(" << keys[i] << ")"; my_log(ss9);
        operation->set(Storage::Operation::GET, keys[i], value);
        //std::stringstream ss10;
        //ss10 << number << " apply get(" << keys[i] << ")"; my_log(ss10);
        flat_combiner->apply_slot();
        bool not_found = operation->error_code() == Storage::ErrorCode::NOT_FOUND;
        //std::stringstream ss5;
        //ss5 << number << " ErrorCode(" << operation->error_code() << ")"; my_log(ss5);
        result = operation->user_slot()->get_data();
        if (!not_found) {
            //std::stringstream ss5;
            //ss5 << number << " Fail, result(" << result << ")"; my_log(ss5);

            abort();
        }
    }

}

TEST(FlatCombineLogicTest, PutGetTest) {
    std::cout << std::unitbuf;

    shared_storage = std::make_shared<Storage>();
    auto shared_flat_combiner = std::make_shared<FlatCombiner::FlatCombiner<StorageSlot>>();

    std::vector<std::thread> workers;
    for (int i = 0; i < workers_number; i++) {
        workers.emplace_back(&put_get_worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}

TEST(FlatCombineLogicTest, PutGetDeleteTest) {
    std::cout << std::unitbuf;

    shared_storage = std::make_shared<Storage>();
    auto shared_flat_combiner = std::make_shared<FlatCombiner::FlatCombiner<StorageSlot>>();

    std::vector<std::thread> workers;
    for (int i = 0; i < workers_number; i++) {
        workers.emplace_back(&put_get_delete_worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}
