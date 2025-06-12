#include <queue>
#include <cassert>

#include "fibers.hpp"

class FiberScheduler {
public:
    friend class Inspector;
    /// Fiber simple trampoline
    friend void trampoline(Fiber *fiber);

    virtual ~FiberScheduler() {
        assert(queue.empty());
    }

    void schedule(Fiber fiber) {
        schedule(create_context_from_fiber(std::move(fiber)));
    }

    void schedule(Context context) {
        queue.push(std::move(context));
    }

    /// Prepare stack, execution, arguments, etc...
    static Context create_context_from_fiber(Fiber fiber);

    /// Reschedule self to end of queue
    static YieldData yield(YieldData);

    template <class Inspector, class... Args>
    void create_current_fiber_inspector(Args... args) {
        sched_context.inspector = std::make_shared<Inspector>(args...);
    }

    bool empty() {
        return queue.empty();
    }

protected:
    /// Proceed one context from queue
    void run_one();

    /// Proceed till queue is not empty
    virtual void run() {
        while (!empty()) {
            run_one();
        }
    }

private:
    std::queue<Context> queue;
    Context sched_context;
};
