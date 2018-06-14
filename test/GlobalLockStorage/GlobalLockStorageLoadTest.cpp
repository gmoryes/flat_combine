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
const int THREADS_NUMBER = 10;

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

    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        EXPECT_TRUE(storage->Execute(Storage::Operation::PUT, keys[i], value) == Storage::ErrorCode::OK);
    }

    std::string result;
    for (int i = 0; i < MAX_OPERATION_PER_THREAD; i++) {
        EXPECT_TRUE(storage->Execute(Storage::Operation::GET, keys[i], result) == Storage::ErrorCode::OK);
        EXPECT_TRUE(result == value);
    }
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
