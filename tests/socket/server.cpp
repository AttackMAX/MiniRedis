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

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        /*
            成功时：返回读取到的字节数（> 0）
            连接关闭时：返回 0（EOF）
            错误时：返回 -1（并设置 errno）
        */
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

const size_t k_max_msg = 4096;

static int32_t one_request(int connfd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }
    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }
    // request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }
    // do something
    printf("client says: %.*s\n", len, &rbuf[4]);
    // reply using the same protocol
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
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
    while(true){
        int32_t err = one_request(connfd);
        if(err){
            break;
        }
    }
    // do_something(connfd);
    close(connfd);
  }
}