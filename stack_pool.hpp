#include <vector>
#include <cstdlib>


class StackPool {
public:
    enum {
        STACK_SIZE = 1024 * 1024 * 4,
    };

    ~StackPool() {
        for (auto elem : stacks) {
            std::free(elem);
        }
    }

    struct Stack {
        StackPool *sp = nullptr;
        void *ptr = nullptr;

        Stack() = default;

        Stack(StackPool *sp, void *ptr) : sp(sp), ptr(ptr) {
        }

        ~Stack() {
            free();
        }

        Stack(Stack &&other) : sp(other.sp), ptr(other.ptr) {
            other.ptr = nullptr;
        }

        Stack &operator=(Stack &&other) noexcept {
            free();
            sp = other.sp;
            std::swap(ptr, other.ptr);
            return *this;
        }

        Stack(const Stack &other) = delete;
        void operator=(const Stack &other) = delete;

        void free() noexcept {
            if (!ptr) {
                return;
            }
            sp->free(ptr);
            ptr = nullptr;
        }
    };

    Stack alloc() {
        if (!stacks.empty()) {
            auto ptr = stacks.back();
            stacks.pop_back();
            return {this, ptr};
        }
        auto ptr = std::calloc(1, STACK_SIZE);
        return {this, ptr};
    }

    void free(void *stack) {
        stacks.push_back(stack);
    }

private:
    std::vector<void *> stacks;
};
