#include "epoll.hpp"
#include <ucontext.h>
#include <exception>
#include <cstring>
#include <cerrno>
#include <fcntl.h>

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


void scheduler_run(EpollScheduler &sched) {
    if (EpollScheduler::current_scheduler) {
        throw std::runtime_error("Global scheduler is not empty");
    }
    EpollScheduler::current_scheduler = &sched;
    ucontext_t main_uc;
    getcontext(&main_uc);
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
    act.action = sched_context.exception ? Action::THROW : Action::START;
    act.user_data = sched_context.yield_data;
    sched_context.yield_data = {};
    auto ret = sched_context.switch_context(act);

    if (sched_context.inspector) {
        (*sched_context.inspector)(ret, sched_context);
    }

    if (ret.action == Action::SCHED) {
        queue.push(std::move(sched_context));
    }
}

void EpollScheduler::await_read(Context context, YieldData data) {
    auto* rd = static_cast<ReadData*>(data.ptr);
    int fd = rd->fd;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    Node node{std::move(context), fd, data, &EpollScheduler::do_read};

    auto &elem = wait_list[fd];
    bool was_empty = !elem.in && !elem.out;
    if (elem.in)
        throw std::runtime_error("duplicate read await");
    elem.in.emplace(std::move(node));

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = 0;
    if (elem.in)
        ev.events |= EPOLLIN;
    if (elem.out)
        ev.events |= EPOLLOUT;

    if (epoll_ctl(epoll_fd, was_empty ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::runtime_error("epoll_ctl failed");
    }
}

void EpollScheduler::do_read(Node node) {
    auto* rd = static_cast<ReadData*>(node.data.ptr);
    ssize_t r = ::read(node.fd, rd->data, rd->size);
    if (r < 0) {
        node.context.exception = std::make_exception_ptr(std::runtime_error(strerror(errno)));
    } else {
        node.context.yield_data.ss = r;
    }
    schedule(std::move(node.context));
}

void EpollScheduler::await_write(Context context, YieldData data) {
    auto* wd = static_cast<WriteData*>(data.ptr);
    int fd = wd->fd;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    Node node{std::move(context), fd, data, &EpollScheduler::do_write};
    auto &elem = wait_list[fd];
    bool was_empty = !elem.in && !elem.out;
    if (elem.out)
        throw std::runtime_error("duplicate write await");
    elem.out.emplace(std::move(node));

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = 0;
    if (elem.in)
        ev.events |= EPOLLIN;
    if (elem.out)
        ev.events |= EPOLLOUT;

    if (epoll_ctl(epoll_fd, was_empty ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::runtime_error("epoll_ctl failed");
    }
}

void EpollScheduler::do_write(Node node) {
    auto* wd = static_cast<WriteData*>(node.data.ptr);
    ssize_t w = ::write(node.fd, wd->data, wd->size);
    if (w < 0) {
        node.context.exception = std::make_exception_ptr(std::runtime_error(strerror(errno)));
    } else {
        node.context.yield_data.ss = w;
    }
    schedule(std::move(node.context));
}

void EpollScheduler::await_accept(Context context, YieldData data) {
    auto* ad = static_cast<AcceptData*>(data.ptr);
    int fd = ad->fd;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    Node node{std::move(context), fd, data, &EpollScheduler::do_accept};
    auto &elem = wait_list[fd];
    bool was_empty = !elem.in && !elem.out;
    if (elem.in)
        throw std::runtime_error("duplicate accept await");
    elem.in.emplace(std::move(node));

    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = 0;
    if (elem.in)
        ev.events |= EPOLLIN;
    if (elem.out)
        ev.events |= EPOLLOUT;

    if (epoll_ctl(epoll_fd, was_empty ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::runtime_error("epoll_ctl failed");
    }
}

void EpollScheduler::do_accept(Node node) {
    auto* ad = static_cast<AcceptData*>(node.data.ptr);
    int r = ::accept(node.fd, ad->addr, ad->addrlen);
    if (r < 0) {
        node.context.exception = std::make_exception_ptr(std::runtime_error(strerror(errno)));
    } else {
        node.context.yield_data.i = r;
        fcntl(r, F_SETFL, fcntl(r, F_GETFL, 0) | O_NONBLOCK);
    }
    schedule(std::move(node.context));
}

void EpollScheduler::do_error(Node node) {
    node.context.exception = std::make_exception_ptr(std::runtime_error("epoll error"));
    schedule(std::move(node.context));
}

void EpollScheduler::run() {
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    while (true) {
        while (!empty()) {
            run_one();
        }
        if (wait_list.empty()) {
            break;
        }
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("epoll_wait failed");
        }
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            auto it = wait_list.find(fd);
            if (it == wait_list.end()) {
                continue;
            }
            auto rec = std::move(it->second);
            if (ev & (EPOLLERR | EPOLLHUP)) {
                if (rec.in) {
                    Node n = std::move(*rec.in);
                    do_error(std::move(n));
                }
                if (rec.out) {
                    Node n = std::move(*rec.out);
                    do_error(std::move(n));
                }
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                wait_list.erase(it);
                continue;
            }
            if ((ev & EPOLLIN) && rec.in) {
                Node n = std::move(*rec.in);
                rec.in.reset();
                (this->*n.callback)(std::move(n));
            }
            if ((ev & EPOLLOUT) && rec.out) {
                Node n = std::move(*rec.out);
                rec.out.reset();
                (this->*n.callback)(std::move(n));
            }
            if (!rec.in && !rec.out) {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                wait_list.erase(it);
            } else {
                struct epoll_event evs{};
                evs.data.fd = fd;
                evs.events = 0;
                if (rec.in)
                    evs.events |= EPOLLIN;
                if (rec.out)
                    evs.events |= EPOLLOUT;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &evs);
                it->second = std::move(rec);
            }
        }
    }
}
