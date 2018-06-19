#include "gtest/gtest.h"
#include "Storage/Storage.h"
#include "FlatCombiner/FlatCombiner.h"
#include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <iostream>
#include <cstdlib>

using namespace Repository;
using shared_combiner_t = std::shared_ptr<FlatCombiner::FlatCombiner<StorageSlot>>;
using shared_storage_t = std::shared_ptr<Storage>;
shared_storage_t shared_storage;

const int MAX_OPERATION_PER_THREAD = 1e5;
const int THREADS_NUMBER = 4;

bool check_error(FlatCombiner::Operation<StorageSlot> *operation, bool must_be = false) {

    if (operation->error_code()) {

        EXPECT_TRUE(must_be);
        return true;
    }

    return false;
}

void put_get_worker(int number, shared_combiner_t flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    FlatCombiner::Operation<StorageSlot> *operation = flat_combiner->get_slot();
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

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        operation->set(Storage::Operation::PUT, keys[i], value);
        flat_combiner->apply_slot();
        check_error(operation);
    }

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        operation->set(Storage::Operation::GET, keys[i]);
        flat_combiner->apply_slot();
        check_error(operation);

        std::string result = operation->user_slot()->get_data();
        EXPECT_TRUE(result == value);
        if (result != value) {

            abort();
        }
    }

    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Time: " << elapsed.count() << "s" << std::endl;
}

void put_get_delete_worker(int number, shared_combiner_t flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    FlatCombiner::Operation<StorageSlot> *operation = flat_combiner->get_slot();
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

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        operation->set(Storage::Operation::PUT, keys[i], value);
        flat_combiner->apply_slot();
        check_error(operation);

        operation->set(Storage::Operation::GET, keys[i], value);
        flat_combiner->apply_slot();
        check_error(operation);

        std::string result = operation->user_slot()->get_data();
        EXPECT_TRUE(result == value);
        if (result != value) {
            abort();
        }

        operation->set(Storage::Operation::DELETE, keys[i], value);
        flat_combiner->apply_slot();
        check_error(operation);

        operation->set(Storage::Operation::GET, keys[i], value);
        flat_combiner->apply_slot();
        bool not_found = operation->error_code() == Storage::ErrorCode::NOT_FOUND;
        result = operation->user_slot()->get_data();
        if (!not_found) {

            abort();
        }
    }

    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Time: " << elapsed.count() << "s" << std::endl;

}

TEST(FlatCombineLogicTest, PutGetTest) {
    std::cout << std::unitbuf;

    shared_storage = std::make_shared<Storage>();
    auto shared_flat_combiner = std::make_shared<FlatCombiner::FlatCombiner<StorageSlot>>();

    std::vector<std::thread> workers;
    for (int i = 0; i < THREADS_NUMBER; i++) {
        workers.emplace_back(&put_get_worker, i, shared_flat_combiner);
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
    for (int i = 0; i < THREADS_NUMBER; i++) {
        workers.emplace_back(&put_get_delete_worker, i, std::ref(shared_flat_combiner));
    }

    for (auto &thread : workers) {
        thread.join();
    }
}
