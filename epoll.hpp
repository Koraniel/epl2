#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>

#include "scheduler.hpp"

struct ReadData {
    int fd;
    char * data;
    size_t size;
};

struct WriteData {
    int fd;
    const char * data;
    size_t size;
};

struct AcceptData {
    int fd;
    sockaddr * addr;
    socklen_t * addrlen;
};

class EpollScheduler : public FiberScheduler {
private:
    struct Node;

    using Callback = void (EpollScheduler::*)(Node node);

    struct Node {
        Context context;
        int fd;
        YieldData data;
        Callback callback = nullptr;
    };

    struct Events {
        std::optional<Node> in;
        std::optional<Node> out;
    };

public:
    /// Start scheduler event loop
    friend void scheduler_run(EpollScheduler &sched);

    static EpollScheduler *current_scheduler;

    EpollScheduler() {
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error("Can not create epoll");
        }
    }

    ~EpollScheduler() override {
        close(epoll_fd);
    }

    void await_read(Context context, YieldData data);

    void do_read(Node node);

    void await_write(Context context, YieldData data);

    void do_write(Node node);

    void await_accept(Context context, YieldData data);

    void do_accept(Node node);

    void do_error(Node node);

    void run() override;

private:
    std::unordered_map<int, Events> wait_list;
    int epoll_fd;
};

void schedule(Fiber fiber);
void yield();
