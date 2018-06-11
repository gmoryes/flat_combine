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
#include <condition_variable>
#include <atomic>

std::mutex mutex321;
void loggggg(std::string str) {
    std::lock_guard<std::mutex> lock(mutex321);
    std::cout << std::hex << str << std::endl;
}

using shared_combiner_t = std::shared_ptr<FlatCombiner<Storage, StorageSlot>>;
const int MAX_OPERATION_PER_THREAD = 1000;

std::atomic<int> alive_workers_number(0);
std::condition_variable cv;
std::mutex mutex;

bool check_error(StorageSlot *storage_slot, bool must_be = false) {
    std::exception ex;
    if (storage_slot->error(ex)) {
        std::cout << "Error: " << ex.what() << std::endl;
        EXPECT_TRUE(must_be);
        return true;
    }

    return false;
}

void worker(int number, shared_combiner_t& flat_combiner) {
    srand(static_cast<unsigned int>(time(0) * number));
    std::stringstream ss1;
    ss1 << number << " get slot"; loggggg(ss1.str());
    StorageSlot *storage_slot = flat_combiner->get_slot();

    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string key = ss.str() + "_key_";
    std::string value = ss.str();

    //int N = MAX_OPERATION_PER_THREAD * (std::abs(rand()) / double(RAND_MAX));
    int N = 1;

    std::vector<std::string> keys(N);
    for (int i = 0; i < N; i++) {
        std::stringstream ss_full;
        ss_full << key << rand();
        keys[i] = ss_full.str();
    }

    for (int i = 0; i < N; i++) {
        std::stringstream ss2;
        ss2 << number << " set put(" << keys[i] << ")"; loggggg(ss2.str());
        storage_slot->set_operation(Storage::Operation::PUT, keys[i], value);
        std::stringstream ss3;
        ss3 << number << " apply put(" << keys[i] << ")"; loggggg(ss3.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);
    }

    for (int i = 0; i < N; i++) {
        std::stringstream ss2;
        ss2 << number << " set get(" << keys[i] << ")"; loggggg(ss2.str());
        storage_slot->set_operation(Storage::Operation::GET, keys[i]);
        std::stringstream ss3;
        ss3 << number << " apply get(" << keys[i] << ")"; loggggg(ss3.str());
        flat_combiner->apply_slot();
        check_error(storage_slot);

        std::string result = storage_slot->get_data();
        std::stringstream ss4;
        ss4 << number << " result(" << result << ")"; loggggg(ss4.str());
        EXPECT_TRUE(result == value);
        if (result != value) {
            std::stringstream ss5;
            ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; loggggg(ss5.str());

            abort();
        }
    }

    flat_combiner->detach();
    alive_workers_number.fetch_sub(1);
    std::stringstream ss4;
    ss4 << number << " do notify"; loggggg(ss4.str());
    cv.notify_one();
}

TEST(FlatCombineLogicTest, PutGetDeleteTest) {
    std::cout << "pid(" << getpid() << ")" << std::endl;
    Storage storage;

    std::function<void(StorageSlot*, StorageSlot*)> optimize_func(StorageSlot::optimize_queue);

    auto shared_flat_combiner = std::make_shared<FlatCombiner<Storage, StorageSlot>>(optimize_func);

    std::vector<std::thread> workers;
    int workers_number = 2;
    int iterations_number = 10;

    std::unique_lock<std::mutex> lock(mutex);

    for (int i = 0; i < iterations_number; i++) {
        if (alive_workers_number < workers_number) {
            for (int j = 0; j < workers_number; j++) {
                std::thread thread(&worker, j, std::ref(shared_flat_combiner));
                alive_workers_number.fetch_add(1);
                thread.detach();
            }
        }
        std::stringstream ss4;
        ss4 << "=========wait_start(" << i << ")=========="; loggggg(ss4.str());
        cv.wait(lock, [&] { return alive_workers_number < workers_number; });
        std::stringstream ss5;
        ss5 << "=========wait_end(" << i << ")=========="; loggggg(ss5.str());
    }

    int current_worker_number;
    while ((current_worker_number = alive_workers_number.load())) {
        cv.wait(lock, [&] { return alive_workers_number != current_worker_number; });
    }
}