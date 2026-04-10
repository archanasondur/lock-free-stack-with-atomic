#include <atomic>
#include <optional>
#include <memory>
#include <iostream>


template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };

    std::atomic<Node*> top_{nullptr};

public:
    void push(T value) {
        Node* new_node = new Node(std::move(value));
        Node* expected = top_.load();
        while (!top_.compare_exchange_weak(expected, new_node)) {
            // expected is auto-updated to current top_ on failure
            // just retry
        }
        new_node->next = expected;
    }

    std::optional<T> pop() {
        Node* expected = top_.load();
        while (true) {
            if (expected == nullptr) return std::nullopt;
            Node* desired = expected->next;
            if (top_.compare_exchange_weak(expected, desired)) {
                T val = std::move(expected->data);
                delete expected;
                return val;
            }
            // expected auto-updated, retry
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

    stack.push(1);
    stack.push(2);
    stack.push(3);

    while (auto val = stack.pop()) {
        std::cout << *val << "\n";
    }

    return 0;
}