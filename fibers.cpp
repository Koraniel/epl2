#include "fibers.hpp"
#include <ucontext.h>

StackPool stack_pool;

// current executing context information
thread_local ucontext_t* current_uc = nullptr;
thread_local Context* current_ctx = nullptr;
thread_local Action current_action{};

void set_current(ucontext_t* uc, Context* ctx) {
    current_uc = uc;
    current_ctx = ctx;
}

Context::Context(Fiber fiber)
        : fiber(std::make_unique<Fiber>(std::move(fiber))),
          stack(stack_pool.alloc()),
          rsp(reinterpret_cast<intptr_t>(stack.ptr) + StackPool::STACK_SIZE) {
}

Context::~Context() {
    delete uc;
}

Context::Context(Context &&other) noexcept
        : fiber(std::move(other.fiber)),
          stack(std::move(other.stack)),
          uc(other.uc),
          rip(other.rip),
          rsp(other.rsp),
          inspector(std::move(other.inspector)),
          exception(std::move(other.exception)),
          yield_data(other.yield_data) {
    other.uc = nullptr;
    other.rip = 0;
    other.rsp = 0;
}

Context &Context::operator=(Context &&other) noexcept {
    if (this != &other) {
        fiber = std::move(other.fiber);
        stack = std::move(other.stack);
        delete uc;
        uc = other.uc;
        rip = other.rip;
        rsp = other.rsp;
        inspector = std::move(other.inspector);
        exception = std::move(other.exception);
        yield_data = other.yield_data;
        other.uc = nullptr;
        other.rip = 0;
        other.rsp = 0;
    }
    return *this;
}

Action Context::switch_context(Action action) {
    auto* to_uc = reinterpret_cast<ucontext_t*>(rip);
    if (!to_uc) {
        throw std::runtime_error("Context not initialized");
    }

    auto* from_uc = current_uc;
    auto* from_ctx = current_ctx;

    current_action = action;
    current_uc = to_uc;
    current_ctx = this;

    swapcontext(from_uc, to_uc);

    auto res = current_action;

    current_uc = from_uc;
    current_ctx = from_ctx;
    return res;
}
