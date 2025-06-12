#include <functional>
#include <cinttypes>
#include <stdexcept>
#include <memory>
#include <optional>

#include "stack_pool.hpp"
#include <ucontext.h>


using Fiber = std::function<void()>;

union YieldData {
    void * ptr = {};
    int32_t i;
    uint32_t u;
    size_t s;
    ssize_t ss;
};
static_assert(sizeof(YieldData) <= sizeof(void*));


struct Action {
    enum {
        /// Scheduler -> Fiber
        START,
        THROW,

        /// Fiber -> Scheduler
        STOP,
        SCHED,
    } action;

    YieldData user_data{};
};


class Inspector;

struct Context {
    std::unique_ptr<Fiber> fiber;
    StackPool::Stack stack;
    ucontext_t *uc = nullptr;

    intptr_t rip = 0;
    intptr_t rsp = 0;
    std::shared_ptr<Inspector> inspector;
    std::exception_ptr exception{};
    YieldData yield_data = {};

    Context() = default;

    ~Context();

    explicit Context(Fiber fiber);

    Context(Context &&other) noexcept;

    Context(const Context &other) = delete;

    Context &operator=(Context &&other) noexcept;

    void operator=(const Context &other) = delete;

    /// swap current rip and rsp
    Action switch_context(Action);
};

class Inspector {
public:
    virtual ~Inspector() = default;

    /// Inspect context after execution
    virtual void operator()(Action &, Context &) = 0;
};
