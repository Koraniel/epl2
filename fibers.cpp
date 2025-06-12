#include "fibers.hpp"

StackPool stack_pool;

Context::Context(Fiber fiber)
        : fiber(std::make_unique<Fiber>(std::move(fiber))),
          stack(stack_pool.alloc()),
          rsp(reinterpret_cast<intptr_t>(stack.ptr) + StackPool::STACK_SIZE) {
}

Action Context::switch_context(Action action) {
    auto *ripp = &rip;
    auto *rspp = &rsp;
    asm volatile(""  /// save rbp
                 ""  /// switch rsp
                 ""  /// switch rip with ret_label
                 /// BEFORE (B) GOING BACK
                 "ret_label:"  /// TODO
                 /// AFTER (A)
                 ""  /// load rbp
            : "+S"(rspp), "+D"(ripp)  /// throw action
            ::"rax", "rbx", "rcx", "rdx", "memory", "cc");
    return action;
}
