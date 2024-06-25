#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 1000
#define PORT 8000

char *ADDR;

void convert(uint8_t *buf, char *str, ssize_t size);

int num_seconds;

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

void *send_msg(void *_fd) {
  int fd = *((int *)_fd);
  time_t start_time, current_time;
  time(&start_time);
  while (1) {
    time(&current_time);
    if (difftime(current_time, start_time) >= num_seconds) {
      break;
    }

    usleep(100 * 1000);

    uint8_t buf[10];
    // first +1 is for \0
    char str[10 * 2 + 1];
    if (getentropy(buf, sizeof(buf)) == -1) {
      handle_error("getentropy");
    }
    convert(buf, str, 21);
    // add 0 type
    char completed_0_msg[10 * 2 + 1 + 1];
    sprintf(completed_0_msg, "0%s", str);
    // TODO: in msg, we dont include \0, we will add it on server
    completed_0_msg[22 - 1] = '\n';
    // FIXME AND TODO: include \n!!!! and need to add \0???
    if (write(fd, completed_0_msg, 22) != 22) {
      handle_error("write");
    }
  }
  // after n seconds, send 1 msg
  if (write(fd, "1\n", strlen("1\n")) != strlen("1\n")) {
    handle_error("write");
  }
  return NULL;
}
int main(int argc, char *argv[]) {
  if (argc != 3) {
    handle_error("Only two agruments needed");
  }
  num_seconds = atoi(argv[1]);
  ADDR = argv[2];
  struct sockaddr_in addr;
  int sfd;
  ssize_t num_read;
  char buf[BUF_SIZE];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) ==
      -1) {
    handle_error("write");
  }
  // TODO: create PTHREAD here
  pthread_t t1;
  if (pthread_create(&t1, NULL, send_msg, (void *)&sfd) != 0) {
    handle_error("pthread_create");
  }
  // main thread recv msg
  while (1) {
    // FOR ALL MSG, ALL ENDING WITH '\n'
    int index = 0;
    // TODO: if buf_size not enough....
    char msg[BUF_SIZE];
    while (1) {
      // FIXME: sfd????
      num_read = read(sfd, &msg[index], sizeof(char));
      if (num_read == -1) {
        handle_error("read");
      }
      // FIXME: == 0???
      if (msg[index] == '\n' || num_read == 0) {
        // FIXME: \n to \0, this is IMPORTANT coz printf following need this,
        // also we should not print \n as part of msg
        msg[index] = '\0';
        break;
      }
      index++;
    }
    assert(index >= 0);

    // FIXME: check type of msg
    if (msg[0] == '1') {
      // type 1
      exit(1);
    } else {
      // type 0 normal msg and print it
      /*
      uint32_t ip;
      uint16_t port;
      const char *real_msg;
      memcpy(&ip, msg + 1, sizeof(ip));
      memcpy(&port, msg + 5, sizeof(port));
      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &ip, ip_str, INET_ADDRSTRLEN);
      port = ntohs(port);
      char port_str[10];
      sprintf(port_str, "%hu", port);
      real_msg = msg + 7;
      */

      char msg_type;
      uint32_t msg_ip;
      uint16_t msg_port;
      char msg_str[BUF_SIZE];

      memcpy(&msg_type, &msg[0], 1);
      memcpy(&msg_ip, &msg[1], 4);
      memcpy(&msg_port, &msg[5], 2);
      memcpy(&msg_str, &msg[7], strlen(msg) - 7);
      msg_str[strlen(msg) - 7] = '\0';

      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &msg_ip, ip_str, INET_ADDRSTRLEN);
      // TODO:
      msg_port = ntohs(msg_port);
      char port_str[10];
      sprintf(port_str, "%hu", msg_port);

      printf("%-20s%-10s%s\n", ip_str, port_str, msg_str);
    }
  }

  // join here
  pthread_join(t1, NULL);
  return 0;
}

/*
 * buf should point to an array that contains random bytes to convert to a hex
 * string.
 *
 * str should point to a buffer used to return the hex string of the random
 * bytes. The size of the buffer should be twice the size of the random bytes
 * (since a byte is two characters in hex) plus one for NULL.
 *
 * size is the size of the str buffer.
 *
 * For example,
 *
 *   uint8_t buf[10];
 *   char str[10 * 2 + 1];
 *   getentropy(buf, 10);
 *   convert(buf, str, 21);
 */
void convert(uint8_t *buf, char *str, ssize_t size) {
  if (size % 2 == 0)
    size = size / 2 - 1;
  else
    size = size / 2;

  for (int i = 0; i < size; i++)
    sprintf(str + i * 2, "%02X", buf[i]);
}
