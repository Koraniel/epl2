#include "async.hpp"

namespace Async {
class AcceptInspector : public Inspector {
public:
    void operator()(Action &act, Context &context) override {
        /// Stop this fiber and async wait for accept
        /// TODO
    }
};

int accept(int fd, sockaddr *addr, socklen_t *addrlen) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    /// TODO
    /// Hint: what happens on yielding this thread? What the "Inspector" is?
}

class ReadInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        /// Stop this fiber and async wait for read
        /// TODO
    }
};

ssize_t read(int fd, char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    /// TODO
}

class WriteInspector : public Inspector {
    void operator()(Action &act, Context &context) override {
        /// Stop this fiber and async wait for write
        /// TODO
    }
};

ssize_t write(int fd, const char *buf, size_t size) {
    if (!EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is empty");
    }
    /// TODO
}
}  // namespace Async