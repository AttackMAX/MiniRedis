// stdlib
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
// C++
#include <string>
#include <vector>

static void die(const char *msg) {
  fprintf(stderr, "[%d] %s\n", errno, msg);
  abort();
}
static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void do_something(int connfd) {
  char rbuf[64] = {};
  ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
  if (n < 0) {
    msg("read() error");
    return;
  }
  printf("client says: %s\n", rbuf);

  char wbuf[] = "world";
  write(connfd, wbuf, strlen(wbuf));
}

int main() {
  // 1.创建套接字
  /*
      AF_INET 用于 IPv4。对于 IPv6 或双栈套接字，请使用 AF_INET6
      SOCK_STREAM 用于 TCP。使用 SOCK_DGRAM 用于 UDP
  */
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  // 2.设置套接字属性
  /*
      SO_REUSEADDR
     应该始终保持设置，若未设置，那么服务器重启后将无法绑定到之前使用的ip地址和端口
  */
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // 3.绑定
  /*
      sockaddr_in 保存一个IPv4端口对
      htons & htonl
     用于将主机字节序转换为网络字节序，htons转换为16位端口号，htonl转换为32位ip地址
  */
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(0);
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }

  // 4.监听端口
  /*
      SOMAXCONN是全连接队列的大小
  */
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }

  // 5.接受连接
  /*

  */
  while (true) {
    sockaddr_in client_addr{};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
      continue;
    }
    do_something(connfd);
    close(connfd);
  }
}