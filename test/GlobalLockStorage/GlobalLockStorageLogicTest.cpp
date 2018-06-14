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

TEST(StorageTest, PutGet) {
    Storage storage;

    std::string key = "key1";
    std::string value = "value1";

    EXPECT_TRUE(storage.Execute(Storage::Operation::PUT, key, value) == Storage::ErrorCode::OK);

    std::string result;
    EXPECT_TRUE(storage.Execute(Storage::Operation::GET, key, result) == Storage::ErrorCode::OK);
    EXPECT_TRUE(result == value);
}

TEST(StorageTest, GetNotExisting) {
    Storage storage;

    std::string key = "some_key_404";

    EXPECT_TRUE(storage.Execute(Storage::Operation::GET, key) == Storage::ErrorCode::NOT_FOUND);
}

TEST(StorageTest, PutDeleteGet) {
    Storage storage;

    std::string key = "key1";
    std::string value = "value1";

    EXPECT_TRUE(storage.Execute(Storage::Operation::PUT, key, value) == Storage::ErrorCode::OK);
    EXPECT_TRUE(storage.Execute(Storage::Operation::DELETE, key) == Storage::ErrorCode::OK);
    EXPECT_TRUE(storage.Execute(Storage::Operation::GET, key) == Storage::ErrorCode::NOT_FOUND);
}

TEST(StorageTest, BadOperation) {
    Storage storage;

    std::string key = "key1";
    std::string value = "value1";

    EXPECT_TRUE(storage.Execute(9999, key, value) == Storage::ErrorCode::UNSUPPORTED_OPERATION);
}