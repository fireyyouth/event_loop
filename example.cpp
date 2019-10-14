#include "Eventloop.hpp"

void chain(Executor &x) {
  x.create_event(0, EPOLLIN, [](Executor &x) {
    auto buf = std::make_shared< std::array<char, 128> >();
    int n = read(0, buf->data(), buf->size());
    if (n < 0) {
      throw std::runtime_error(strerror(errno));
    } else if (n == 0) {
        return;
    }
    x.create_event(1, EPOLLOUT, [ n, buf ](Executor & x) {
      write(1, buf->data(), n);
      chain(x);
    });
  });
}

int main() {
  Executor x;
  chain(x);
  x.run();
}
