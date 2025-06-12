#include "async.hpp"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>
#include <sstream>
#include <cstring>
#include <random>

void test_simple() {
    std::cout << __FUNCTION__ << std::endl;

    int x = 0;

    EpollScheduler sched;

    sched.schedule([&]() {
        ++x;
        std::cout << "Done" << std::endl;
    });

    scheduler_run(sched);

    assert(x == 1);
}

void test_multiple() {
    std::cout << __FUNCTION__ << std::endl;

    int x = 0;

    EpollScheduler sched;

    sched.schedule([&]() {
        ++x;
        std::cout << "Done" << std::endl;
    });
    sched.schedule([&]() {
        ++x;
        std::cout << "Done" << std::endl;
    });
    sched.schedule([&]() {
        ++x;
        std::cout << "Done" << std::endl;
    });

    scheduler_run(sched);

    assert(x == 3);
}

void test_recursive() {
    std::cout << __FUNCTION__ << std::endl;

    int x = 0;

    EpollScheduler sched;

    sched.schedule([&]() {
        schedule([&]() {
            ++x;
            std::cout << "Done" << std::endl;
        });
    });
    sched.schedule([&]() {
        schedule([&]() {
            schedule([&]() {
                ++x;
                std::cout << "Done" << std::endl;
            });
        });
    });
    sched.schedule([&]() {
        schedule([&]() {
            schedule([&]() {
                schedule([&]() {
                    ++x;
                    std::cout << "Done" << std::endl;
                });
            });
        });
    });

    scheduler_run(sched);

    assert(x == 3);
}

enum {
    ITERS = 10,
};

void test_yield_one() {
    std::cout << __FUNCTION__ << std::endl;

    int x = 0;

    EpollScheduler sched;

    sched.schedule([&]() {
        for (int i = 0; i != ITERS; ++i) {
            ++x;
            yield();
        }
        std::cout << "Done" << std::endl;
    });

    assert(x == 0);

    scheduler_run(sched);

    assert(x == ITERS);
}

void test_yield_many() {
    std::cout << __FUNCTION__ << std::endl;

    int x = 0;
    int cur_fiber = -1;

    EpollScheduler sched;

    auto create_fiber = [&](int fiber_id) {
        return [&, fiber_id]() {
            for (int i = 0; i != ITERS; ++i) {
                assert(cur_fiber != fiber_id);
                cur_fiber = fiber_id;
                ++x;
                yield();
            }
            std::cout << "Done" << std::endl;
        };
    };

    sched.schedule(create_fiber(1));
    sched.schedule(create_fiber(2));
    sched.schedule(create_fiber(3));

    assert(x == 0);

    scheduler_run(sched);

    assert(x == 3 * ITERS);
}

int prepare_listen_sock(short port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    sockaddr_in addr = {AF_INET};
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    assert(bind(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
    assert(listen(sock, 16) == 0);
    return sock;
}

int prepare_client_sock(short port) {
    sockaddr_in addr = {AF_INET};
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    assert(connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
    return sock;
}

void write_all(int fd, const char * data, size_t size) {
    while (size > 0) {
        auto w = Async::write(fd, data, size);
        assert(w > 0);
        size -= w;
        data += w;
    }
}

void test_simple_server_client() {
    std::cout << __FUNCTION__ << std::endl;

    constexpr short port = 8080;

    auto server = [=](){
        int sock = prepare_listen_sock(port);
        auto client = Async::accept(sock, nullptr, nullptr);
        std::vector<char> msg(100);
        auto r = Async::read(client, msg.data(), msg.size());
        assert(r > 0);
        std::cout << msg.data() << std::endl;
        auto w = Async::write(client, msg.data(), r);
        assert(r == w);
        close(client);
        close(sock);
    };

    auto client = [](){
        int sock = prepare_client_sock(port);
        std::string msg = "Hello, hell!";
        auto w = Async::write(sock, msg.data(), msg.size());
        assert(w == msg.size());
        std::string res(100, '\0');
        auto r = Async::read(sock, res.data(), res.size());
        assert(r == msg.size());
        assert(msg == res.substr(0, msg.size()));
        std::cout << msg << std::endl;
    };

    EpollScheduler sched;

    sched.schedule(server);
    sched.schedule(client);

    scheduler_run(sched);
}

void test_server_many_clients() {
    std::cout << __FUNCTION__ << std::endl;

    constexpr short port = 8080;
    constexpr int clients = 5;

    auto server = [=](){
        auto sock = prepare_listen_sock(port);

        for (int i = 0; i != clients; ++i) {
            auto client_fd = Async::accept(sock, nullptr, nullptr);
            auto echo = [=]() {
                std::vector<char> buf(1024);
                while (true) {
                    auto r = Async::read(client_fd, buf.data(), buf.size());
                    if (r == 0) {
                        break;
                    }
                    assert(r > 0);
                    write_all(client_fd, buf.data(), r);
                }
                close(client_fd);
            };
            schedule(echo);
        }
        close(sock);
    };

    auto client = [=](){
        std::vector<int> socks;
        for (int i = 0; i != clients; ++i) {
            socks.push_back(prepare_client_sock(port));
        }
        std::string msg = "This is text message";
        for (auto && sock : socks) {
            auto w = Async::write(sock, msg.data(), msg.size());
            assert(w == msg.size());
        }
        for (auto && sock : socks) {
            std::string res(100, '\0');
            auto r = Async::read(sock, res.data(), res.size());
            assert(r == msg.size());
            assert(msg == res.substr(0, msg.size()));
        }
        for (auto && sock : socks) {
            close(sock);
        }
        std::cout << "Done" << std::endl;
    };

    EpollScheduler sched;

    sched.schedule(server);
    sched.schedule(client);

    scheduler_run(sched);
}

void test_server_many_clients2() {
    std::cout << __FUNCTION__ << std::endl;

    constexpr short port = 8080;
    constexpr int clients = 5;

    auto server = [=](){
        auto sock = prepare_listen_sock(port);

        for (int i = 0; i != clients; ++i) {
            auto client_fd = Async::accept(sock, nullptr, nullptr);
            auto echo = [=]() {
                std::vector<char> buf(1024);
                while (true) {
                    auto r = Async::read(client_fd, buf.data(), buf.size());
                    if (r == 0) {
                        break;
                    }
                    assert(r > 0);
                    write_all(client_fd, buf.data(), r);
                }
                close(client_fd);
            };
            schedule(echo);
        }
        close(sock);
    };

    auto client = [=](){
        auto sock = prepare_client_sock(port);
        std::string msg = "This is text message";
        auto w = Async::write(sock, msg.data(), msg.size());
        yield();
        assert(w == msg.size());
        std::string res(100, '\0');
        auto r = Async::read(sock, res.data(), res.size());
        yield();
        yield();
        assert(r == msg.size());
        assert(msg == res.substr(0, msg.size()));
        close(sock);
        yield();
        yield();
        yield();
        std::cout << "Done" << std::endl;
    };

    EpollScheduler sched;

    sched.schedule(server);
    for (int i = 0; i != clients; ++i) {
        sched.schedule(client);
    }

    scheduler_run(sched);
}

void ping(int fd) {
    char data = 'c';
    assert(Async::write(fd, &data, 1) == 1);
}


void test_exceptionaltest() {
    std::cout << __FUNCTION__ << std::endl;

    constexpr short port_reader = 8080;
    constexpr short port_writer = 8088;

    auto reader = [=]() {
        auto sock = prepare_listen_sock(port_reader);
        auto client_fd = Async::accept(sock, nullptr, nullptr);
        std::string buf(1024, '\0');
        assert(Async::read(client_fd, buf.data(), buf.size()) == 0);
        ping(client_fd);
        try {
            Async::read(client_fd, buf.data(), buf.size());
        } catch (std::exception& e) {
            std::cout << "Done" << std::endl;
        }
        close(client_fd);
        auto writer_fd = Async::accept(sock, nullptr, nullptr);
        std::string msg = "Hello!";
        auto r = Async::read(writer_fd, buf.data(), buf.size());
        assert(r == msg.size());
        assert(msg == buf.substr(0, msg.size()));
        close(writer_fd);
        close(sock);
        std::cout << "Done" << std::endl;
    };

    auto writer = [=]() {
        auto sock = prepare_listen_sock(port_writer);
        auto client_fd = Async::accept(sock, nullptr, nullptr);
        ping(client_fd);
        std::string buf = "Hello!";
        try {
            Async::write(client_fd, buf.data(), buf.size());
        } catch (std::exception& e) {
            std::cout << "Done" << std::endl;
        }
        close(client_fd);
        close(sock);
        auto reader_fd = prepare_client_sock(port_reader);
        auto w = Async::write(reader_fd, buf.data(), buf.size()); 
        assert(w == buf.size());
        close(reader_fd);
        std::cout << "Done" << std::endl;
    };

    auto thrower = [] () {
        throw std::runtime_error("Thrower threw");
    };

    auto badboy = [](short port) {
        auto sock = prepare_client_sock(port);
        close(sock);
    };

    EpollScheduler sched;

    sched.schedule(reader);
    sched.schedule(writer);
    sched.schedule([badboy]() { badboy(port_reader); });
    sched.schedule([badboy]() { badboy(port_writer); });
    sched.schedule(thrower);

    std::string excepted = "Thrower threw";
    try {
        scheduler_run(sched);
    } catch (std::exception &e) {
        assert(excepted == std::string(e.what()));
    }
    scheduler_run(sched);
    std::cout << "Done" << std::endl;
}

void test_supertest() {
    std::cout << __FUNCTION__ << std::endl;

    constexpr short port = 8080;
    constexpr short proxy_port = 8088;
    constexpr int clients = 10;

    auto proxy_server = [=](){
        auto sock = prepare_listen_sock(proxy_port);

        for (int i = 0; i != clients; ++i) {
            auto client_fd = Async::accept(sock, nullptr, nullptr);
            auto server_fd = prepare_client_sock(port);
            auto client_to_server = [=]() {
                std::vector<char> buf(1024);
                try {
                    while (true) {
                        auto r = Async::read(client_fd, buf.data(), buf.size());
                        if (r == 0) {
                            break;
                        }
                        assert(r > 0);
                        write_all(server_fd, buf.data(), r);
                    }
                } catch (std::exception &e) {
                }
                shutdown(client_fd, SHUT_RD);
                shutdown(server_fd, SHUT_WR);
                close(client_fd);
                close(server_fd);
            };
            auto server_to_client = [=]() {
                std::vector<char> buf(1024);
                try {
                    while (true) {
                        auto r = Async::read(server_fd, buf.data(), buf.size());
                        if (r == 0) {
                            break;
                        }
                        assert(r > 0);
                        write_all(client_fd, buf.data(), r);
                    }
                } catch (std::exception &e) {
                }
                shutdown(server_fd, SHUT_RD);
                shutdown(client_fd, SHUT_WR);
                close(server_fd);
                close(client_fd);
            };
            schedule(client_to_server);
            schedule(server_to_client);
        }
        close(sock);
    };

    std::unordered_map<std::string, std::string> database;

    class Input {
    public:
        Input(int fd) : fd(fd) {};

        std::string get_line() {
            bool eof = false;
            while (true) {
                assert(last != buf.size());
                for (size_t i = 0; i != last; ++i) {
                    if (buf[i] == '\n') {
                        std::string line(buf.data(), i);
                        if (i + 1 != last) {
                            std::string new_buf(buf.data() + i + 1, last - line.size() - 1);
                            memcpy(buf.data(), new_buf.data(), new_buf.size());
                        }
                        last -= line.size() + 1;
                        return line;
                    }
                }
                if (!eof) {
                    auto r = Async::read(fd, buf.data() + last, buf.size() - last);
                    if (r == 0) {
                        eof = true;
                        continue;
                    }
                    assert(r > 0);
                    last += r;
                } else {
                    std::string line(buf.data(), last);
                    last = 0;
                    return line;
                }
            }
        }

    private:
        int fd;
        std::array<char, 1024> buf;
        size_t last = 0;
    };

    auto server = [=, &database](){
        auto sock = prepare_listen_sock(port);

        for (int i = 0; i != clients; ++i) {
            auto client_fd = Async::accept(sock, nullptr, nullptr);
            auto client = [=, &database]() {
                Input input(client_fd);
                while (true) {
                    std::string line;
                    try {
                        line = input.get_line();
                    } catch (...) {
                        break;
                    }
                    if (line.empty()) {
                        break;
                    }
                    std::istringstream iss(line);
                    std::string cmd;
                    iss >> cmd;
                    if (cmd == "STOP") {
                        break;
                    }
                    std::string key;
                    iss >> key;
                    std::ostringstream oss;
                    if (cmd == "GET") {
                        auto it = database.find(key);
                        if (it == database.end()) {
                            oss << "None";
                        } else {
                            oss << it->second;
                        }
                    } else if (cmd == "PUT") {
                        std::string val;
                        iss >> val;
                        database[key] = val;
                        oss << "Ok";
                    }
                    oss << '\n';
                    auto res = oss.str();
                    write_all(client_fd, res.data(), res.size());
                }
                close(client_fd);
            };
            schedule(client);
        }
        close(sock);
    };

    std::vector<std::string> keys = {
            "Key1",
            "Key2",
            "Key3",
            "Key4",
            "Key5",
    };
    int num = 0;

    auto client = [&](){
        auto sock = prepare_client_sock(proxy_port);
        auto self_id = std::to_string(num++);
        Input input(sock);
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, keys.size() - 1);
        for (int i = 0; i != 100; ++i) {
            auto r = dist(rng);
            std::string cmd;
            if (r == 0) {
                cmd = "GET " + keys[dist(rng)];
            } else if (r == 1) {
                cmd = "PUT " + keys[dist(rng)] + " " + self_id;
            } else if (r == 2) {
                cmd = "GET " + self_id + keys[dist(rng)];
            } else if (r == 3) {
                cmd = "PUT " + self_id + keys[dist(rng)] + " " + self_id;
            } else {
                continue;
            }
            cmd.push_back('\n');
            write_all(sock, cmd.data(), cmd.size());
            input.get_line();
        }
        close(sock);
    };

    auto client0 = [&](){
        auto sock = prepare_client_sock(proxy_port);
        Input input(sock);
        auto doo = [&](std::string cmd, std::string ans){
            cmd.push_back('\n');
            write_all(sock, cmd.data(), cmd.size());
            auto res = input.get_line();
            assert(ans == res);
        };
        doo("GET A", "None");
        doo("GET B", "None");
        doo("PUT A 10", "Ok");
        doo("GET A", "10");
        doo("GET B", "None");
        doo("PUT A 20", "Ok");
        doo("GET A", "20");
        close(sock);
    };

    EpollScheduler sched;

    sched.schedule(proxy_server);
    sched.schedule(server);
    sched.schedule(client0);
    for (int i = 1; i != clients; ++i) {
        sched.schedule(client);
    }

    scheduler_run(sched);
    std::cout << "Done" << std::endl;
}

int main() {
    test_simple();
    test_multiple();
    test_recursive();
    test_yield_one();
    test_yield_many();
    test_simple_server_client();
    test_server_many_clients();
    test_server_many_clients2();
    test_exceptionaltest();
    test_supertest();
}
