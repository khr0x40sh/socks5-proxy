#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CONN 128
#define BUF 4096

typedef struct {
    int client;
    int remote;
} conn_t;

conn_t conns[MAX_CONN];

void add_conn(int c, int r) {
    for (int i=0;i<MAX_CONN;i++) {
        if (conns[i].client == 0) {
            conns[i].client = c;
            conns[i].remote = r;
            return;
        }
    }
}

void remove_conn(int i) {
    close(conns[i].client);
    close(conns[i].remote);
    conns[i].client = 0;
    conns[i].remote = 0;
}

int connect_target(unsigned char *buf) {

    if (buf[3] == 1) { // IPv4

        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));

        addr.sin_family = AF_INET;
        memcpy(&addr.sin_addr, buf+4,4);

        int port = (buf[8]<<8) | buf[9];
        addr.sin_port = htons(port);

        int s = socket(AF_INET, SOCK_STREAM,0);
        if (connect(s,(struct sockaddr*)&addr,sizeof(addr))<0)
            return -1;

        return s;
    }

    if (buf[3] == 3) { // domain

        char host[256];
        int len = buf[4];

        memcpy(host,buf+5,len);
        host[len]=0;

        int port = (buf[5+len]<<8) | buf[6+len];

        struct hostent *he = gethostbyname(host);
        if (!he) return -1;

        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        memcpy(&addr.sin_addr,he->h_addr,he->h_length);

        int s = socket(AF_INET, SOCK_STREAM,0);
        if (connect(s,(struct sockaddr*)&addr,sizeof(addr))<0)
            return -1;

        return s;
    }

    return -1;
}

void handle_socks(int client) {

    unsigned char buf[BUF];

    read(client,buf,2);
    read(client,buf,buf[1]);

    unsigned char resp[2]={0x05,0x00};
    write(client,resp,2);

    int n = read(client,buf,BUF);
    if (n<=0) {
        close(client);
        return;
    }

    int remote = connect_target(buf);
    if (remote <0) {
        close(client);
        return;
    }

    unsigned char ok[10]={0x05,0x00,0x00,0x01,0,0,0,0,0,0};
    write(client,ok,10);

    add_conn(client,remote);
}

int main(int argc,char *argv[]) {

    int port=1080;
    if (argc>1) port=atoi(argv[1]);

    int server = socket(AF_INET,SOCK_STREAM,0);

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));

    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;

    bind(server,(struct sockaddr*)&addr,sizeof(addr));
    listen(server,10);

    fd_set rfds;
    char buf[BUF];

    while (1) {

        FD_ZERO(&rfds);
        FD_SET(server,&rfds);

        int maxfd = server;

        for (int i=0;i<MAX_CONN;i++) {
            if (conns[i].client) {
                FD_SET(conns[i].client,&rfds);
                FD_SET(conns[i].remote,&rfds);

                if (conns[i].client>maxfd) maxfd=conns[i].client;
                if (conns[i].remote>maxfd) maxfd=conns[i].remote;
            }
        }

        if (select(maxfd+1,&rfds,NULL,NULL,NULL)<0)
            continue;

        if (FD_ISSET(server,&rfds)) {

            int client = accept(server,NULL,NULL);
            if (client>0)
                handle_socks(client);
        }

        for (int i=0;i<MAX_CONN;i++) {

            if (!conns[i].client) continue;

            int c = conns[i].client;
            int r = conns[i].remote;

            if (FD_ISSET(c,&rfds)) {

                int n = read(c,buf,BUF);
                if (n<=0) { remove_conn(i); continue; }

                write(r,buf,n);
            }

            if (FD_ISSET(r,&rfds)) {

                int n = read(r,buf,BUF);
                if (n<=0) { remove_conn(i); continue; }

                write(c,buf,n);
            }
        }
    }
}
