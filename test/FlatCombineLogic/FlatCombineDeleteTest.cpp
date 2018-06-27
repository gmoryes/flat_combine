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
#include <condition_variable>
#include <atomic>
#include <sys/time.h>

using namespace Repository;
using shared_combiner_t = std::shared_ptr<FlatCombiner::FlatCombiner<StorageSlot>>;
using shared_storage_t = std::shared_ptr<Storage>;
const int MAX_OPERATION_PER_THREAD = 1e5;
const int WORKERS_NUMBER = 4;

std::atomic<int> alive_workers_number(0);
std::condition_variable cv;
std::mutex mutex;
shared_storage_t shared_storage;


bool check_error(StorageSlot *operation, bool must_be = false) {

    if (operation->error_code) {

        EXPECT_TRUE(must_be);
        return true;
    }

    return false;
}

void worker(int number, shared_combiner_t& flat_combiner) {

    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    srand(static_cast<unsigned int>(ms * number));
    StorageSlot *operation = flat_combiner->get_slot();
    operation->init(shared_storage);

    std::stringstream ss;
    ss << std::this_thread::get_id();

    std::string key = ss.str() + "_key_";
    std::string value = ss.str();

    int N = (MAX_OPERATION_PER_THREAD * (std::abs(rand()) / double(RAND_MAX)));
    //int N = 1;

    std::vector<std::string> keys(N);
    for (int i = 0; i < N; i++) {
        std::stringstream ss_full;
        ss_full << key << rand();
        keys[i] = ss_full.str();
    }

    for (int i = 0; i < N; i++) {
        operation->prepare_data(keys[i], value);
        flat_combiner->apply_slot(Storage::Operation::PUT);
        check_error(operation);
    }

    for (int i = 0; i < N; i++) {
        operation->prepare_data(keys[i]);
        flat_combiner->apply_slot(Storage::Operation::GET);
        check_error(operation);

        std::string result = operation->get_data();
        EXPECT_TRUE(result == value);
        if (result != value) {

            abort();
        }
    }

    flat_combiner->detach();
    alive_workers_number.fetch_sub(1);
    cv.notify_one();
}

TEST(FlatCombineLogicTest, PutGetDeleteTest) {
    std::cout << std::unitbuf;

    shared_storage = std::make_shared<Storage>();

    auto shared_flat_combiner = std::make_shared<FlatCombiner::FlatCombiner<StorageSlot>>();

    std::vector<std::thread> workers;
    int iterations_number = 10;
    std::unique_lock<std::mutex> lock(mutex);
    int tmp = 0;
    for (int i = 0; i < iterations_number; i++) {
        if (alive_workers_number < WORKERS_NUMBER) {
            std::thread thread(&worker, tmp, std::ref(shared_flat_combiner));
            tmp++;
            alive_workers_number.fetch_add(1);
            thread.detach();
        }
        cv.wait(lock, [&] { return alive_workers_number < WORKERS_NUMBER; });
    }

    int current_worker_number;
    while ((current_worker_number = alive_workers_number.load())) {
        cv.wait(lock, [&] { return alive_workers_number != current_worker_number; });
    }
}