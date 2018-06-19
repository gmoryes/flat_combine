# Flat Combiner

## Introduction

Here my realization of FlatCombiner, original algorithm colud be read [here](https://people.csail.mit.edu/shanir/publications/Flat%20Combining%20SPAA%2010.pdf).
FlatCombiner's code placed at **[FlatCombiner.h](https://github.com/gmoryes/flat_combine/blob/master/src/FlatCombiner/FlatCombiner.h)**.

## What for?

FlatCombiner could be useful in case of concurrent access to some 
structure, for example to Array. To avoid data races and undefined 
behaviours there is simple method - use lock on structure. But if you 
have lot's of threads workers and high load application, the concurrent
access to your structure can be a bottleneck in the work of the program. To solve such
type of problems we have FlatCombiner.

## Algorithm in short

Let's name *SharedStructure* - the structure, which we will access parallel.

### We have
1. Common object - *FlatCombiner*
2. Class *SharedStructureSlot* - class where we will store request data

**Note:** *SharedStructureSlot* will be a field of FlatCombiner object as 
thread local variable, so each thread has it's *SharedStructureSlot*.

### Algorithm
1. FlatCombiner keep alive lock free queue of thread local slots.
2. Each thread worker receive it's thread local slot. Set there some 
operation and say FlatCombiner to execute it.
3. In time of execution, only one thread became executor. He became combiner,
and run throw the queue and get all slots where exists data to execute. Other threads
will get into spin lock, waiting combiner.

## Infrastructure

Okay, let's see what the classes we have, and how it use.
Start from *SharedStructureSlot*. It must provide next API:

1. First of all we need method, that FlatCombiner could call for prepare
request data: `void prepate_data(args...)`.
2. Next, we must have `execute()` method. This function called by combiner, after
receive all slots with data for execute. It has next prototype: 
    ```cpp
    using task_type = std::pair<SharedStructureSlot*, int>;
    template <std::size_t SHOT_N>
    void execute(std::array<task_type, SHOT_N> &tasks, size_t n) {
        /* 
            Code of execution pack of tasks
            You can execute your tasks as you want, for example:
            
            for (size_t i = 0; i < n; i++) {
                auto task = tasks.at(i).first;
                int op_code = tasks.at(i).second;
                switch (op_code) {
                    case SET:
                        break;
                    // etc...
                }
            }
        */
    }
    ```
    Where:
     
     `task_type` - is pair of pointer to *SharedStructureSlot* and integer 
    number - code of operation.
    
    `tasks` - array of tasks.
    
    `SHOT_N` - maximum number of tasks per call. (Set in *FlatCombiner*, 
    need just for *std::array*).
    
    `n` - current tasks number.
    
3. *SharedStructureSlot* must have integer field - *error_code*. Here combiner
will store the result of execution (OK or some error code).

**Note:** Code of operation must start from **one**. Because zero operation code
means - **no** operation set in slot.

Okay, now we know the API of *SharedStructureSlot*. Let's talk about API of 
FlatCombiner.

1. Constructor. `FlatCombiner<SharedStructureSlot, SHOT_N>()`. *SharedStructureSlot*
is already familiar class. `SHOT_N` is size of maximum number of tasks in one 
combine shot.
2. Each slot must do `FlatCombine::get_slot()` to receive thread local 
slot with whom it will work. But this method return not 
*SharedStructureSlot*. It returns pointer to slot wrapper, called 
`FlatCombiner::Operation<SharedStructureSlot>`.
3. `FlatCombiner::Operation<SharedStructureSlot>` provides next API:
    1. Method to set operation into slot.
        ```cpp
        FlatCombiner::Operation<SharedStructureSlot>::set(int op_code, args...)
        ```
        `op_code` - code of operation.
        
        `args...` - this arguments **passed** to suitable `prepare_data()` method in
        *SharedStructureSlot*.
    2. Get error code after execution of opearation.
        ```cpp
        FlatCombiner::Operation<SharedStructureSlot>::error_code()
        ```
    3. Get pointer to *SharedStructureSlot*, storead into slot wrapper.
        ```cpp
        FlatCombiner::Operation<SharedStructureSlot>::user_slot()
        ```
4. Apply operation. Here we go into spin lock, until somebody (or 
current thread, if he is lucky) execute last operation.
    ```cpp
    FlatCombiner::FlatCombiner<SharedStructureSlot, SHOT_N>::apply_slot()
    ```
    
    
# Example
That all. Let's write simple example. Our *SharedStructure* will 
be an `Array<Type, SIZE>`. Let's implement *SharedStructureSlot*.
```cpp
template <typename Type, std::size_t SIZE>
class MultiThreadArraySlot {

    using task_type = std::pair<SharedStructureSlot*, int>;

    template <std::size_t SHOT_N>
    void execute(std::array<task_type, SHOT_N> &tasks, size_t n);
    
    void prepare_data(int index);
    void prepare_data(int index, const Type &value);
    
    int error_code;
    
    // Useful method
    void init(const std::shared_ptr<MultiThreadArray<Type, SIZE>> &storage);
    
private:
    std::shared_ptr<MultiThreadArray<Type, SIZE>> _storage;
    Type _data;
    int _index;
} 
```

`MultiThreadArray<Type, SIZE>` is some structure, which represent 
an Array of type Type and size of SIZE.

In slot I defined `_data` field, where will store the value of data 
with type `Type`. And `_index` for number of array cell.

Also define *must have* API methods (see upper desc for more details).

About `init()` method: note, that after do flat_combine->get_slot(), 
we received empty slot, so use want to initialize it.

Implementation `prepare_data`, we just set inner fields:
```cpp
void prepare_data(int index) {
    _index = index;
}

void prepare_data(int index, const Type &value) {
    prepare_data(index);
    _data = value;
}
```

Implementation `init()`, we just initialize a `_storage` field.
```cpp
void init(const std::shared_ptr<MultiThreadArray<Type, SIZE>> &storage) {
    _storage = std::move(storage);
}
```

Implementation `execute()`, we just pass query to our _storage:
```cpp
template <std::size_t SHOT_N>
void execute(std::array<task_type, SHOT_N> &tasks, size_t n) {
    _storage->Execute(tasks, n);
}
```

We use some method `Execute()` of `_storage` object. It looks like:
```cpp
enum ErrorCode {
    OK,
    BAD_INDEX // etc...
}

template <std::size_t SHOT_N>
void Execute(std::array<task_type, SHOT_N> &tasks, size_t n) {
    
    MultiThreadArraySlot<Type, SIZE> *slot;
    int op_code;
    
    for (size_t i = 0; i < n; i++) {
        std::tie(slot, op_code) = tasks.at(i);
        
        switch (op_code) {
             // Code ...
             slot->error_code = Operation::OK;
        }
    }
}
```

You can find full code of these classes at **[MultiThreadArray.h](https://github.com/gmoryes/flat_combine/blob/master/src/Array/MultiThreadArray.h)**.

The next step - how to use it with *FlatCombiner*.
Create an array of type `int` and size `64` and `flat_combine`.
```cpp
// master

using multithread_array_slot_type = MultiThreadArraySlot<int, 64>;

auto array = std::make_shared<MultiThreadArray<int, 64>>();
auto flat_combine = std::make_shared<
                        FlatCombiner<multithread_array_slot_type>();

```

```cpp
// worker
// Get our ThreadLocal slot
FlatCombiner::Operation<multithread_array_slot_type> *slot = flat_combine->get_slot();

// Initialize it by our array
slot->user_slot()->init(array);

// Create some request to slot

// multithread_array_type::Operation::SET is just and integer number, 
// defined in multithread_array_type
int index = 5;
int value = 42;
slot->set(multithread_array_type::Operation::SET, index, value);

// Say FlatCombiner to execute it
flat_combine->apply_slot();

// Check that all ok
bool success = slot->error_code() == multithread_array_type::ErrorCode::OK);
```

The full usage with all cases could be find at **[ArrayUsageExample.cpp](https://github.com/gmoryes/flat_combine/blob/master/test/ArrayUsageExample/ArrayUsageExample.cpp)**.
