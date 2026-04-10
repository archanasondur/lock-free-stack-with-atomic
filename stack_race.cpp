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
        Node* expected = top_.load();
        while (true) {
            new_node->next = expected;
            if (top_.compare_exchange_weak(expected, new_node)) break;
        }
    }

    optional<T> pop() {
        Node* expected = top_.load();
        while (true) {
            if (expected == nullptr) return nullopt;
            Node* desired = expected->next;
            if (top_.compare_exchange_weak(expected, desired)) {
                T val = std::move(expected->data);
                delete expected;
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
    }
};

int main() {
    LockFreeStack<int> stack;
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100000;

    // 4 threads all pushing and popping simultaneously
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