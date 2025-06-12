#include "epoll.hpp"

namespace Async {
    int accept(int fd, sockaddr * addr, socklen_t * addrlen);
    ssize_t read(int fd, char * data, size_t size);
    ssize_t write(int fd, const char * data, size_t size);
}
