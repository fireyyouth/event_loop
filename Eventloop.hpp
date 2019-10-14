#pragma once

#include <arpa/inet.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <execinfo.h>

void debug() {
  int nptrs;
  void *buffer[100];
  nptrs = backtrace(buffer, 100);
  char **strings = backtrace_symbols(buffer, nptrs);
  std::cout << std::string(20, '>') << std::endl;
  for (int j = 0; j < nptrs; j++) {
    std::cout << strings[j] << std::endl;
  }
  std::cout << std::string(20, '<') << std::endl;
  free(strings);
}

class Executor {
public:
  Executor() { epollfd_ = epoll_create1(0); }

  Executor(const Executor &) = delete;
  Executor(Executor &&) = delete;
  Executor &operator=(const Executor &) = delete;
  Executor &operator=(Executor &&) = delete;

  template <typename Callable>
  void create_event(int fd, uint32_t events, Callable f) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    // debug();
    std::cerr << "add " << fd << std::endl;
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
      throw std::runtime_error(strerror(errno));
    }
    fd_to_callback_[fd] = f;
  }
  void run() {
    while (!fd_to_callback_.empty()) {
      std::array<struct epoll_event, 128> events;
      int n = epoll_wait(epollfd_, events.data(), events.size(), -1);
      if (n < 0) {
        throw std::runtime_error(strerror(errno));
      }
      for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        // debug();
        std::cerr << "del " << fd << std::endl;
        if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
          throw std::runtime_error(strerror(errno));
        }
        auto f = fd_to_callback_.at(fd);
        fd_to_callback_.erase(fd);
        f();
      }
    }
  }

private:
  using Callback = std::function<void(void)>;
  std::unordered_map<int, Callback> fd_to_callback_;
  int epollfd_;
};

class Socket {
public:
  Socket(const Socket &) = delete;
  Socket(Socket &&) = delete;
  Socket &operator=(const Socket &) = delete;
  Socket &operator=(Socket &&) = delete;
  Socket(Executor &exec, int f) : x(exec), fd(f) {}

  template <typename Callable>
  void async_read(char *buf, size_t len, Callable f) {
    x.create_event(fd, EPOLLIN, [=]() {
      auto n = read(fd, buf, len);
      f(n);
    });
  }

  template <typename Callable>
  void async_write(char *buf, size_t len, Callable f) {
    x.create_event(fd, EPOLLOUT, [=]() {
      auto n = write(fd, buf, len);
      f(n);
    });
  }

  ~Socket() { close(fd); }

public:
  Executor &x;
  int fd;
};

class Listener {
public:
  Listener(const Listener &) = delete;
  Listener(Listener &&) = delete;
  Listener &operator=(const Listener &) = delete;
  Listener &operator=(Listener &&) = delete;

  Listener(Executor &exec, int port) : x(exec) {
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 128);
  }

  template <typename Callable> void async_accept(Callable f) {
    std::cerr << __func__ << std::endl;
    x.create_event(listenfd, EPOLLIN, [=]() {
      int fd = accept(listenfd, NULL, NULL);
      auto *p = new Socket(x, fd);
      f(p);
    });
  }

private:
  Executor &x;
  int listenfd;
};
