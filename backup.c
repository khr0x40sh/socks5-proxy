#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFSIZE 4096

void relay(int a, int b) {
    fd_set fds;
    char buf[BUFSIZE];

    while (1) {
        FD_ZERO(&fds);
        FD_SET(a, &fds);
        FD_SET(b, &fds);

        int max = (a > b ? a : b) + 1;

        if (select(max, &fds, NULL, NULL, NULL) < 0)
            break;

        if (FD_ISSET(a, &fds)) {
            int n = read(a, buf, BUFSIZE);
            if (n <= 0) break;
            write(b, buf, n);
        }

        if (FD_ISSET(b, &fds)) {
            int n = read(b, buf, BUFSIZE);
            if (n <= 0) break;
            write(a, buf, n);
        }
    }
}

int connect_target(unsigned char *buf, int *port) {
    if (buf[3] == 1) { // IPv4
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr, buf + 4, 4);

        *port = (buf[8] << 8) | buf[9];
        addr.sin_port = htons(*port);

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            return -1;
        return s;
    }

    if (buf[3] == 3) { // domain
        char host[256];
        int len = buf[4];

        memcpy(host, buf + 5, len);
        host[len] = 0;

        *port = (buf[5 + len] << 8) | buf[6 + len];

        struct hostent *he = gethostbyname(host);
        if (!he) return -1;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(*port);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            return -1;

        return s;
    }

    return -1;
}

void handle_client(int client) {
    unsigned char buf[BUFSIZE];

    read(client, buf, 2);
    read(client, buf, buf[1]);

    unsigned char reply[2] = {0x05, 0x00};
    write(client, reply, 2);

    int n = read(client, buf, BUFSIZE);
    if (n <= 0) return;

    int port;
    int remote = connect_target(buf, &port);
    if (remote < 0) {
        close(client);
        return;
    }

    unsigned char resp[10] = {0x05,0x00,0x00,0x01,0,0,0,0,0,0};
    write(client, resp, 10);

    relay(client, remote);

    close(remote);
    close(client);
}

int main(int argc, char *argv[]) {
    int port = 1080;
    if (argc > 1)
        port = atoi(argv[1]);

    int server = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 10);

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;

        if (!fork()) {
            close(server);
            handle_client(client);
            exit(0);
        }

        close(client);
    }
}
