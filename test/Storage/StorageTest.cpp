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

    std::string key = "key1";
    std::string value = "value1";
    EXPECT_TRUE(storage.Execute(Storage::Operation::PUT, key, value));

    std::string result;
    EXPECT_TRUE(storage.Execute(Storage::Operation::GET, key, result));
    EXPECT_TRUE(value == result);
}

TEST(StorageTest, GetNotExisting) {
    Storage storage;

    std::string key = "some_key_404";
    std::string result;
    EXPECT_FALSE(storage.Execute(Storage::Operation::GET, key, result));
}

TEST(StorageTest, PutDeleteGet) {
    Storage storage;

    std::string key = "key1";
    std::string value = "value1";
    EXPECT_TRUE(storage.Execute(Storage::Operation::PUT, key, value));

    std::string result;
    EXPECT_TRUE(storage.Execute(Storage::Operation::DELETE, key));

    EXPECT_FALSE(storage.Execute(Storage::Operation::GET, key, result));
}