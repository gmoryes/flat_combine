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
#include "Logger/Logger.h"

using namespace Repository;
using shared_combiner_t = std::shared_ptr<FlatCombiner::FlatCombiner<StorageSlot>>;
using shared_storage_t = std::shared_ptr<Storage>;
const int MAX_OPERATION_PER_THREAD = 1000;

std::atomic<int> alive_workers_number(0);
std::condition_variable cv;
std::mutex mutex;
shared_storage_t shared_storage;


bool check_error(FlatCombiner::Operation<StorageSlot> *operation, bool must_be = false) {

    std::stringstream ss;
    ss << "ErrorCode(" << operation << "): " << operation->error_code() << ", must_error(" << must_be << ")"; my_log(ss);

    if (operation->error_code()) {

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
    std::stringstream ss1;
    ss1 << number << " get slot"; my_log(ss1);
    FlatCombiner::Operation<StorageSlot> *operation = flat_combiner->get_slot();
    operation->user_slot()->init(shared_storage);

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
        std::stringstream ss2;
        ss2 << number << " set put(" << keys[i] << ")"; my_log(ss2);
        operation->set(Storage::Operation::PUT, keys[i], value);
        std::stringstream ss3;
        ss3 << number << " apply put(" << keys[i] << ")"; my_log(ss3);
        flat_combiner->apply_slot();
        check_error(operation);
    }

    for (int i = 0; i < N; i++) {
        std::stringstream ss2;
        ss2 << number << " set get(" << keys[i] << ")"; my_log(ss2);
        operation->set(Storage::Operation::GET, keys[i]);
        std::stringstream ss3;
        ss3 << number << " apply get(" << keys[i] << ")"; my_log(ss3);
        flat_combiner->apply_slot();
        check_error(operation);

        std::string result = operation->user_slot()->get_data();
        std::stringstream ss4;
        ss4 << number << " result(" << result << ")"; my_log(ss4);
        EXPECT_TRUE(result == value);
        if (result != value) {
            std::stringstream ss5;
            ss5 << number << " Fail, result(" << result << "), value(" << value << ")"; my_log(ss5);

            abort();
        }
    }

    flat_combiner->detach();
    alive_workers_number.fetch_sub(1);
    std::stringstream ss4;
    ss4 << number << " do notify"; my_log(ss4);
    cv.notify_one();
}

TEST(FlatCombineLogicTest, PutGetDeleteTest) {
    std::cout << "pid(" << getpid() << ")" << std::endl;
    std::cout << std::unitbuf;

    shared_storage = std::make_shared<Storage>();

    auto shared_flat_combiner = std::make_shared<FlatCombiner::FlatCombiner<StorageSlot>>();

    std::vector<std::thread> workers;
    int workers_number = 3;
    int iterations_number = 10;
    std::cout << "get lock" << std::endl;
    std::unique_lock<std::mutex> lock(mutex);
    std::cout << "get lock done" << std::endl;
    int tmp = 0;
    for (int i = 0; i < iterations_number; i++) {
        if (alive_workers_number < workers_number) {
            std::stringstream ss4;
            ss4 << "=========create new worker(" << tmp << ")=========="; my_log(ss4);
            std::thread thread(&worker, tmp, std::ref(shared_flat_combiner));
            tmp++;
            alive_workers_number.fetch_add(1);
            thread.detach();
        }
        std::stringstream ss4;
        ss4 << "=========wait_start(" << i << ")=========="; my_log(ss4);
        cv.wait(lock, [&] { return alive_workers_number < workers_number; });
        std::stringstream ss5;
        ss5 << "=========wait_end(" << i << ")=========="; my_log(ss5);
    }

    int current_worker_number;
    while ((current_worker_number = alive_workers_number.load())) {
        cv.wait(lock, [&] { return alive_workers_number != current_worker_number; });
    }

    std::stringstream ss5;
    ss5 << "[=======DONE========]"; my_log(ss5);
}