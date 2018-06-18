#include "gtest/gtest.h"
#include "GlobalLockStorage/GlobalLockStorage.h"
#include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <iostream>

using namespace GlobalLockStorage;

const int MAX_OPERATION_PER_THREAD = 1e5;
const int THREADS_NUMBER = 4;

void worker(std::shared_ptr<Storage> storage, int number) {
    srand(static_cast<unsigned int>(time(0) * number));

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
        EXPECT_TRUE(storage->Execute(Storage::Operation::PUT, keys[i], value) == Storage::ErrorCode::OK);
    }

    std::string result;
    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        EXPECT_TRUE(storage->Execute(Storage::Operation::GET, keys[i], result) == Storage::ErrorCode::OK);
        EXPECT_TRUE(result == value);
    }

    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Time: " << elapsed.count() << "s" << std::endl;
}

TEST(StorageLoadTest, PutGet) {
    auto shared_storage = std::make_shared<Storage>();

    std::vector<std::thread> threads(THREADS_NUMBER);

    for (int i = 0; i < THREADS_NUMBER; i++) {
        threads[i] = std::thread(&worker, shared_storage, i);
    }

    for (auto &thread : threads) {
        thread.join();
    }
}
