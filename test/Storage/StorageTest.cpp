#include "gtest/gtest.h"
#include "storage/Storage.h"
#include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <iostream>

using namespace Repository;

TEST(StorageTest, PutGet) {
    Storage storage;
    std::array<task_type, 1> request;

    std::string key = "key1";
    std::string value = "value1";

    StorageSlot slot_put;
    slot_put.prepare_data(key, value);
    request.at(0) = { &slot_put, Storage::Operation::PUT };

    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_put.error_code == Storage::ErrorCode::OK);

    StorageSlot slot_get;
    slot_get.prepare_data(key);
    request.at(0) = { &slot_get, Storage::Operation::GET };

    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_get.error_code == Storage::ErrorCode::OK);
    EXPECT_TRUE(slot_get.get_data() == value);
}

TEST(StorageTest, GetNotExisting) {
    Storage storage;
    std::array<task_type, 1> request;

    std::string key = "some_key_404";

    StorageSlot slot_get;
    slot_get.prepare_data(key);
    request.at(0) = { &slot_get, Storage::Operation::GET };

    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_get.error_code == Storage::ErrorCode::NOT_FOUND);
}

TEST(StorageTest, PutDeleteGet) {
    Storage storage;
    std::array<task_type, 1> request;

    std::string key = "key1";
    std::string value = "value1";

    StorageSlot slot_put;
    slot_put.prepare_data(key, value);
    
    request.at(0) = { &slot_put, Storage::Operation::PUT };
    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_put.error_code == Storage::ErrorCode::OK);

    request.at(0) = { &slot_put, Storage::Operation::DELETE };
    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_put.error_code == Storage::ErrorCode::OK);

    request.at(0) = { &slot_put, Storage::Operation::GET };
    storage.Execute<1>(request, 1);
    EXPECT_TRUE(slot_put.error_code == Storage::ErrorCode::NOT_FOUND);
}