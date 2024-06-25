#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1000
#define PORT 8000
#define LISTEN_BACKLOG 32

int MAX_EVENTS;

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main(int argc, char *argv[]) {
  if (argc != 2) {
    handle_error("Only two agruments needed");
  }
  MAX_EVENTS = atoi(argv[1]);

  struct sockaddr_in addr, remote_addr;
  int sfd, cfd, epollfd;
  int nfds;
  ssize_t num_read;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  char buf[BUF_SIZE];
  struct epoll_event ev, events[MAX_EVENTS];

  int finished_events[MAX_EVENTS];
  for (int i = 0; i < MAX_EVENTS; i++) {
    finished_events[i] = 0;
  }
  // initize connected_fds
  int connected_fds[MAX_EVENTS];
  for (int i = 0; i < MAX_EVENTS; i++) {
    connected_fds[i] = -1;
  }

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    handle_error("epoll_create1");
  }

  // we want to monitor read and write
  ev.events = EPOLLIN | EPOLLOUT;
  ev.data.fd = sfd; // save the accept socket
  // adding the first sfd to epollfd
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
    handle_error("epoll_ctl");
  }

  while (1) {
    // nfds is num of fd with ready events
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      handle_error("epoll_wait");
    }

    for (int i = 0; i < nfds; ++i) {
      // if current event is related to listening socket.
      if (events[i].data.fd == sfd) {
        memset(&remote_addr, 0, sizeof(struct sockaddr_in));
        cfd = accept(sfd, (struct sockaddr *)&remote_addr, &addrlen);
        if (cfd == -1) {
          handle_error("accept");
        }

        // add remote into connected_fds
        for (int i = 0; i < MAX_EVENTS; ++i) {
          if (connected_fds[i] == -1) {
            connected_fds[i] = cfd;
            break;
          }
        }

        // set 0_NONBLOCK
        int flags = fcntl(cfd, F_GETFL, 0);
        if (flags == -1) {
          handle_error("fcntl");
        }
        flags |= O_NONBLOCK;
        if (fcntl(cfd, F_SETFL, flags) == -1) {
          handle_error("fcntl");
        }

        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.fd = cfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cfd, &ev) == -1) {
          perror("epoll_ctl: connect_sock");
          exit(EXIT_FAILURE);
        }
      }
      // read and write
      else {
        // read until EOF
        while ((num_read = read(events[i].data.fd, buf, BUF_SIZE)) > 0) {
          // FOR ALL MSG, ALL ENDING WITH '\n'
          if (num_read == -1) {
            // FIXME: is this ok?
            connected_fds[i] = -1;
            continue;
          }
          int index = 0;
          // TODO: if buf_size not enough....
          while (1) {
            // FIXME == 0???
            if (buf[index] == '\n' || num_read == 0) {
              buf[index] = '\0';
              break;
            }
            index++;
          }
          assert(index > 0);
          // FIXME:
          uint8_t msg_type = atoi(&buf[0]);
          write(STDOUT_FILENO, buf, strlen(buf));
          printf("%s", buf);
          if (msg_type == 0) {
            char completed_msg[BUF_SIZE];
            uint32_t ip = remote_addr.sin_addr.s_addr;
            uint16_t port = remote_addr.sin_port;
            // int ret_sprintf =
            //     sprintf(completed_msg, "%c%" PRIu32 "%" PRIu16 "%s",
            //     msg_type,
            //             ip, port, msg);
            int ret_sprintf = snprintf(completed_msg, BUF_SIZE, "%d%u%hu%s\n",
                                       msg_type, ip, port, buf + 1);
            write(STDOUT_FILENO, completed_msg, strlen(completed_msg));
            if (ret_sprintf < 0) {
              handle_error("sprintf");
            }
            // TODO: write to all clinets

            for (int i = 0; i < MAX_EVENTS; ++i) {
              // TODO:
              if (connected_fds[i] != -1) {
                // we need \0 for strlen.
                if (write(connected_fds[i], completed_msg,
                          strlen(completed_msg)) != strlen(completed_msg)) {
                  handle_error("write");
                }
              }
            }

            /*
            printf("%s", completed_msg);
            for (int i = 0; i < nfds; i++)
              write(events[i].data.fd, completed_msg, strlen(completed_msg));
          */
          } else if (msg_type == 1) {
            // TODO: there is still '/n'
            finished_events[i] = 1;
            int all_finished = 1;

            for (int i = 0; i < MAX_EVENTS; ++i) {
              if (connected_fds[i] != -1 && finished_events[i] == 0) {
                all_finished = 0;
                break;
              }
            }

            if (all_finished == 1) {
              // FIXME & TODO: ending msg should be 1 or "1\n"?
              char *end_msg = "1\n";
              for (int i = 0; i < MAX_EVENTS; i++) {
                if (connected_fds[i] != -1) {
                  if (write(connected_fds[i], end_msg, strlen(end_msg)) !=
                      strlen(end_msg)) {
                    handle_error("write");
                  }
                  if (close(connected_fds[i]) == -1) {
                    handle_error("close");
                  }
                }
              }
              if (close(sfd) == -1) {
                handle_error("close");
              }
              if (close(epollfd) == -1) {
                handle_error("close");
              }
              exit(EXIT_SUCCESS);
            }
          } else {
            perror("not 1 or 0 msg");
          }
          // ending of new changes.
          memset(buf, 0, BUF_SIZE);
        }
      }
    }
  }
  if (close(cfd) == -1) {
    handle_error("close");
  }
}
