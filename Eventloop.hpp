#pragma once

#include <iostream>
#include <memory>
#include <list>
#include <functional>
#include <unordered_map>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

class Executor {
public:
  Executor() { epollfd_ = epoll_create1(0); }
  template <typename Callable>
  void create_event(int fd, uint32_t events, Callable f) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
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
          if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            throw std::runtime_error(strerror(errno));
          }
          fd_to_callback_.at(fd)(*this);
          fd_to_callback_.erase(fd);
        }
    }
  }

private:
  using Callback = std::function<void(Executor &)>;
  std::unordered_map<int, Callback> fd_to_callback_;
  int epollfd_;
};


