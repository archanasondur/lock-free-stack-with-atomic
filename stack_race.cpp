#include <atomic>
#include <optional>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
using namespace std;

template<typename T>
class LockFreeStack {
private:
    // struct Node {
    //     T data;
    //     Node* next;
    //     Node(T val) : data(std::move(val)), next(nullptr) {}
    // };
    struct Node {
        T data;
        Node* next;
        Node* retired_next;  // separate field for retired list
        Node(T val) : data(std::move(val)), next(nullptr), retired_next(nullptr) {}
    };

    atomic<Node*> top_{nullptr};
    atomic<Node*> retired_{nullptr};  // deleted nodes go here

    // void retire(Node* node) {
    //     Node* old = retired_.load(memory_order_relaxed);
    //     while (true) {
    //         node->next = old;
    //         if (retired_.compare_exchange_weak(old, node,
    //             memory_order_release, memory_order_relaxed)) break;
    //     }
    // }
    void retire(Node* node) {
        Node* old = retired_.load(memory_order_relaxed);
        while (true) {
            node->retired_next = old;
            if (retired_.compare_exchange_weak(old, node,
                memory_order_release, memory_order_relaxed)) break;
        }
    }

public:
    void push(T value) {
        Node* new_node = new Node(std::move(value));
        Node* expected = top_.load(memory_order_relaxed);
        while (true) {
            new_node->next = expected;
            if (top_.compare_exchange_weak(expected, new_node,
                memory_order_release, memory_order_relaxed)) break;
        }
    }

    optional<T> pop() {
        Node* expected = top_.load(memory_order_acquire);
        while (true) {
            if (expected == nullptr) return nullopt;
            Node* desired = expected->next;
            if (top_.compare_exchange_weak(expected, desired,
                memory_order_acquire, memory_order_acquire)) {
                T val = std::move(expected->data);
                retire(expected);  // don't delete immediately, defer it
                return val;
            }
        }
    }

    ~LockFreeStack() {
        Node* current = top_.load();
        while (current) {
            Node* next = current->next;
            delete current;
            current = next;
        }
        current = retired_.load();
        while (current) {
            Node* next = current->retired_next;
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