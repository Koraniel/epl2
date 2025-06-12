#include "async.hpp"

namespace Async {
class AcceptInspector : public Inspector {
public:
    void operator()(Action &act, Context &context) override {
        (void)act;
        (void)context;
    }
};

int accept(int fd, sockaddr *addr, socklen_t *addrlen) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    throw std::runtime_error("async not implemented");
}

class ReadInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        (void)act;
        (void)context;
    }
};

ssize_t read(int fd, char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    throw std::runtime_error("async not implemented");
}

class WriteInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        (void)act;
        (void)context;
    }
};

ssize_t write(int fd, const char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    throw std::runtime_error("async not implemented");
}
}  // namespace Async

