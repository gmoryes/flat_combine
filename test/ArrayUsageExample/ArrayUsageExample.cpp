#include "gtest/gtest.h"
#include "Array/MultiThreadArray.h"
#include "FlatCombiner/FlatCombiner.h"
#include <thread>
#include <vector>
#include <memory>

using namespace Array;

using multithread_array_type = MultiThreadArray<int, 64>;
using multithread_array_slot_type = MultiThreadArraySlot<int, 64>;
using flat_combine_type = FlatCombiner::FlatCombiner<multithread_array_slot_type>;

const int WORKERS_NUMBER = 4;

void worker(const std::shared_ptr<flat_combine_type> &flat_combine,
            const std::shared_ptr<multithread_array_type> &array,
            int thread_number) {

    // Get our ThreadLocal slot
    multithread_array_slot_type *slot = flat_combine->get_slot();

    // Initialize it by our array
    slot->init(array);
    
    // Set some request to slot (1)
    int index = thread_number; // Number from 0 to WORKERS_NUMBER
    int value = 42;
    slot->prepare_data(index, value);

    // Say FlatCombiner to execute it
    flat_combine->apply_slot(multithread_array_type::Operation::SET);

    // Check that all ok
    EXPECT_TRUE(slot->error_code == multithread_array_type::ErrorCode::OK);

    // Set another request (2)
    slot->prepare_data(index);

    // Apply it
    flat_combine->apply_slot(multithread_array_type::Operation::GET);

    // Check that all ok
    EXPECT_TRUE(slot->error_code == multithread_array_type::ErrorCode::OK);

    // Get data
    int result = slot->data();

    // Expect we received our value
    EXPECT_TRUE(result == value);

    // Set another request (3)
    slot->prepare_data(index);

    // Apply it
    flat_combine->apply_slot(multithread_array_type::Operation::SET_TO_ZERO);

    // Check that all ok
    EXPECT_TRUE(slot->error_code == multithread_array_type::ErrorCode::OK);

    // Lets check that array[index] == 0
    slot->prepare_data(index);
    flat_combine->apply_slot(multithread_array_type::Operation::GET);

    result = slot->data();

    // Expect that it is ZERO
    EXPECT_TRUE(result == 0);

    // Lets check error handler
    index = -1;
    slot->prepare_data(index);
    flat_combine->apply_slot(multithread_array_type::Operation::GET);
    EXPECT_TRUE(slot->error_code == multithread_array_type::ErrorCode::BAD_INDEX);

    // Lets check another error handler
    index = 0;
    int unsupported_operation = -1;

    slot->prepare_data(index);
    flat_combine->apply_slot(unsupported_operation);
    EXPECT_TRUE(slot->error_code == multithread_array_type::ErrorCode::UNSUPPORTED_OPERATION);

    flat_combine->detach();
}

TEST(ArrayUsageExample, Example) {
    std::cout << std::unitbuf;
    auto array = std::make_shared<multithread_array_type>();
    auto flat_combine = std::make_shared<flat_combine_type>();

    std::vector<std::thread> workers(WORKERS_NUMBER);

    for (int i = 0; i < WORKERS_NUMBER; i++) {
        workers[i] = std::thread(&worker, flat_combine, array, i);
    }

    for (auto &worker : workers) {
        worker.join();
    }
}