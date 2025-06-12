#include "epoll.hpp"

EpollScheduler* EpollScheduler::current_scheduler = nullptr;

void trampoline(Fiber *fiber) {
    /// process exceptions with std::current_exception()
    /// TODO
    (*fiber)();

    EpollScheduler::current_scheduler->sched_context.switch_context(Action{Action::STOP});
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

    /// TODO
    /// stack
    /// function

    return context;
}

YieldData FiberScheduler::yield(YieldData data) {
    /// current_scheduler->sched_context
    /// If THROW -> throw current_scheduler->sched_context.exception
    /// TODO
}

void FiberScheduler::run_one() {
    sched_context = std::move(queue.front());
    queue.pop();

    /// run with START or THROW if exception
    /// except if exception with std::rethrow_exception
    /// inspect if inspector
    /// schedule again if SCHED
    /// TODO
}

void EpollScheduler::await_read(Context context, YieldData data) {
    /// Subscribe epoll
    auto fd = static_cast<ReadData *>(data.ptr)->fd;
    auto &elem = wait_list[fd];
    /// TODO
}

void EpollScheduler::do_read(Node node) {
    /// call read
    /// schedule fiber
    /// TODO
}

void EpollScheduler::await_write(Context context, YieldData data) {
    auto fd = static_cast<WriteData *>(data.ptr)->fd;
    auto &elem = wait_list[fd];
    /// TODO
}

void EpollScheduler::do_write(Node node) {
    /// TODO
}

void EpollScheduler::await_accept(Context context, YieldData data) {
    auto fd = static_cast<AcceptData *>(data.ptr)->fd;
    auto &elem = wait_list[fd];
    /// TODO
}

void EpollScheduler::do_accept(Node node) {
    /// TODO
}

void EpollScheduler::do_error(Node node) {
    /// "throw" exception in context
    /// TODO
}

void EpollScheduler::run() {
    while (true) {
        while (!empty()) {
            run_one();
        }
        if (wait_list.empty()) {
            break;
        }
        /// Wait any fd
        /// If error do_error
        /// Else if in or out process it
        /// TODO
    }
}
