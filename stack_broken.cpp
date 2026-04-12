#include <atomic>
#include <optional>
#include <iostream>
#include <thread>
#include <vector>
using namespace std;

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };

    atomic<Node*> top_{nullptr};

public:
    void push(T value) {
        Node* new_node = new Node(std::move(value));
        Node* expected = top_.load(memory_order_relaxed);
        while (true) {
            new_node->next = expected;
            if (top_.compare_exchange_weak(expected, new_node,
                memory_order_relaxed, memory_order_relaxed)) break;
        }
    }

    optional<T> pop() {
        Node* expected = top_.load(memory_order_relaxed);
        while (true) {
            if (expected == nullptr) return nullopt;
            Node* desired = expected->next;
            if (top_.compare_exchange_weak(expected, desired,
                memory_order_relaxed, memory_order_relaxed)) {
                T val = std::move(expected->data);
                delete expected;
                return val;
            }
        }
    }

    ~LockFreeStack() {
        Node* current = top_.load(memory_order_relaxed);
        while (current) {
            Node* next = current->next;
            delete current;
            current = next;
        }
    }
};

int main() {
    LockFreeStack<int> stack;
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100000;

    vector<thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&stack, i]() {
            for (int j = 0; j < OPS_PER_THREAD; j++) {
                stack.push(i * OPS_PER_THREAD + j);
                stack.pop();
            }
        });
    }

    for (auto& t : threads) t.join();

    cout << "done, no crash\n";
    return 0;
}