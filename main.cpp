#include "Eventloop.hpp"

struct EchoContext {
  std::array<char, 128> buf;
  std::unique_ptr<Socket> sock;
};

void echo_chain(std::shared_ptr<EchoContext> ctx) {
  ctx->sock->async_read(ctx->buf.data(), ctx->buf.size(), [ctx](int n) {
    if (n < 0) {
      throw std::runtime_error(strerror(errno));
    } else if (n == 0) {
      return;
    }
    ctx->sock->async_write(ctx->buf.data(), n, [ctx](int n) {
      if (n < 0) {
        throw std::runtime_error(strerror(errno));
      }
      echo_chain(ctx);
    });
  });
}
void accept_chain(Listener &listener) {
  static int cnt = 5;
  listener.async_accept([&listener](Socket *d) {
    auto ctx = std::make_shared<EchoContext>();
    ctx->sock.reset(d);
    echo_chain(ctx);
    if (--cnt > 0) {
        accept_chain(listener);
    }
  });
}

int main() {
  Executor x;
  Listener listener(x, 5000);
  accept_chain(listener);
  x.run();
}
