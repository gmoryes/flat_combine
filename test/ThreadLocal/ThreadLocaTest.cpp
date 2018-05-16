#include "gtest/gtest.h"
#include <FlatCombiner/FlatCombiner.h>
#include <sstream>
#include <thread>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <iostream>

ThreadLocal<std::thread::id> a;
std::mutex mutex;
void worker(std::stringstream &ss) {

    auto id = std::this_thread::get_id();
    a.set(&id);

    mutex.lock();
    ss << id << " ";
    mutex.unlock();
}

TEST(ThreadLocalTest, Simple) {
    std::vector<std::thread> workers;
    std::stringstream ss;

    for (int i = 0; i < 1000; i++) {
        workers.emplace_back(&worker, std::ref(ss));
    }

    for (auto &thread : workers) {
        thread.join();
    }

    std::set<std::string> set;
    std::string s;

    while (ss >> s) {
        if (set.find(s) != set.end())
            EXIT_FAILURE;

        set.insert(s);
    }

    EXIT_SUCCESS;
}

ThreadLocal<int> b;
std::atomic<int> counter;
void* worker2(void *func_ptr) {
    std::function<void(void*)> func = *(std::function<void(void*)>*)(func_ptr);
    auto id = std::this_thread::get_id();
    int value = 123;
    b = ThreadLocal<int>(&value, func);
}

void destructor(void *x) {
    counter.fetch_add(1);
}
TEST(ThreadLocalTest, WithDestructor) {

    std::function<void(void*)> f(destructor);
    int value = 123;
    b = ThreadLocal<int>(&value, f);
    counter.store(0);

    std::vector<std::thread> workers;
    std::stringstream ss;

    for (int i = 0; i < 1000; i++) {
        workers.emplace_back(&worker2, (void*)(&f));
    }

    for (auto &thread : workers) {
        thread.join();
    }

    EXPECT_TRUE(counter.load() == 1000);
}

TEST(ThreadLocalTest, GetSet) {
    ThreadLocal<std::string> local;
    std::string value = "value";
    local.set(&value);

    std::string *result = static_cast<std::string*>(local.get());

    EXPECT_TRUE(*result == "value");

}