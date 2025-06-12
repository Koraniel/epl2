#include "epoll.hpp"
#include <ucontext.h>
#include <exception>

extern thread_local ucontext_t* current_uc;
extern thread_local Context* current_ctx;
extern thread_local Action current_action;
void set_current(ucontext_t* uc, Context* ctx);

static Context scheduler_context;

EpollScheduler* EpollScheduler::current_scheduler = nullptr;

void trampoline(Fiber *fiber) {
    try {
        (*fiber)();
    } catch (...) {
        scheduler_context.exception = std::current_exception();
    }

    scheduler_context.switch_context(Action{Action::STOP});
    __builtin_unreachable();
}

void schedule(Fiber fiber) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    EpollScheduler::current_scheduler->schedule(std::move(fiber));
}

void yield() {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    EpollScheduler::current_scheduler->yield({});
}

template <class Inspector, class... Args>
void create_current_fiber_inspector(Args... args) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    EpollScheduler::current_scheduler->create_current_fiber_inspector<Inspector>(args...);
}

void scheduler_run(EpollScheduler &sched) {
    if (EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is not empty");
    }
    EpollScheduler::current_scheduler = &sched;
    ucontext_t main_uc;
    scheduler_context.rip = reinterpret_cast<intptr_t>(&main_uc);
    set_current(&main_uc, &scheduler_context);
    try {
        EpollScheduler::current_scheduler->run();
    } catch (...) {
        EpollScheduler::current_scheduler = nullptr;
        throw;
    }
    EpollScheduler::current_scheduler = nullptr;
}

Context FiberScheduler::create_context_from_fiber(Fiber fiber) {
    Context context(std::move(fiber));

    auto* uc = new ucontext_t;
    getcontext(uc);
    uc->uc_stack.ss_sp = context.stack.ptr;
    uc->uc_stack.ss_size = StackPool::STACK_SIZE;
    uc->uc_link = nullptr;
    makecontext(uc, (void(*)())trampoline, 1, context.fiber.get());
    context.rip = reinterpret_cast<intptr_t>(uc);

    return context;
}

YieldData FiberScheduler::yield(YieldData data) {
    auto act = scheduler_context.switch_context({Action::SCHED, data});
    if (act.action == Action::THROW) {
        std::rethrow_exception(scheduler_context.exception);
    }
    return act.user_data;
}

void FiberScheduler::run_one() {
    auto ctx = std::move(queue.front());
    queue.pop();

    Action act;
    act.action = ctx.exception ? Action::THROW : Action::START;
    auto ret = ctx.switch_context(act);

    if (ctx.inspector) {
        (*ctx.inspector)(ret, ctx);
    }

    if (ret.action == Action::SCHED) {
        queue.push(std::move(ctx));
    }
}

void EpollScheduler::await_read(Context context, YieldData data) {
    (void)context;
    (void)data;
}

void EpollScheduler::do_read(Node node) {
    (void)node;
}

void EpollScheduler::await_write(Context context, YieldData data) {
    (void)context;
    (void)data;
}

void EpollScheduler::do_write(Node node) {
    (void)node;
}

void EpollScheduler::await_accept(Context context, YieldData data) {
    (void)context;
    (void)data;
}

void EpollScheduler::do_accept(Node node) {
    (void)node;
}

void EpollScheduler::do_error(Node node) {
    (void)node;
}

void EpollScheduler::run() {
    FiberScheduler::run();
}
