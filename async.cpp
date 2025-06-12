#include "async.hpp"

namespace Async {
class AcceptInspector : public Inspector {
public:
    void operator()(Action &act, Context &context) override {
        if (act.action == Action::SCHED) {
            context.inspector.reset();
            EpollScheduler::current_scheduler->await_accept(std::move(context), act.user_data);
            act.action = Action::STOP;
        }
    }
};

int accept(int fd, sockaddr *addr, socklen_t *addrlen) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    auto *data = new AcceptData{fd, addr, addrlen};
    create_current_fiber_inspector<AcceptInspector>();
    YieldData yd{};
    yd.ptr = data;
    auto res = FiberScheduler::yield(yd);
    delete data;
    return res.i;
}

class ReadInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        if (act.action == Action::SCHED) {
            context.inspector.reset();
            EpollScheduler::current_scheduler->await_read(std::move(context), act.user_data);
            act.action = Action::STOP;
        }
    }
};

ssize_t read(int fd, char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    auto *data = new ReadData{fd, buf, size};
    create_current_fiber_inspector<ReadInspector>();
    YieldData yd{};
    yd.ptr = data;
    auto res = FiberScheduler::yield(yd);
    delete data;
    return res.ss;
}

class WriteInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        if (act.action == Action::SCHED) {
            context.inspector.reset();
            EpollScheduler::current_scheduler->await_write(std::move(context), act.user_data);
            act.action = Action::STOP;
        }
    }
};

ssize_t write(int fd, const char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    auto *data = new WriteData{fd, buf, size};
    create_current_fiber_inspector<WriteInspector>();
    YieldData yd{};
    yd.ptr = data;
    auto res = FiberScheduler::yield(yd);
    delete data;
    return res.ss;
}
}  // namespace Async

