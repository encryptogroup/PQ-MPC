#ifndef NET_IO_H__
#define NET_IO_H__
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <queue>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define NETWORK_BUFFER_SIZE 1024*16

namespace emp {
class NetIO{
public:
    bool is_server;
    int mysocket = -1;
    int consocket = -1;
    FILE * stream = nullptr;
    char * buffer = nullptr;
    bool has_sent = false;
    std::string addr;
    int port;
    uint64_t send_counter = 0;
    uint64_t recv_counter = 0;

    NetIO(const char* address, int port) {
        this->port = port;
        is_server = (address == nullptr);
        if (address == nullptr) {
            struct sockaddr_in dest;
            struct sockaddr_in serv;
            socklen_t socksize = sizeof(struct sockaddr_in);
            memset(&serv, 0, sizeof(serv));
            serv.sin_family = AF_INET;
            serv.sin_addr.s_addr = htonl(INADDR_ANY); /* set our address to any interface */
            serv.sin_port = htons(port);           /* set the server port number */
            mysocket = socket(AF_INET, SOCK_STREAM, 0);
            int reuse = 1;
            setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
            if(::bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) < 0) {
                perror("error: bind");
                exit(1);
            }
            if(listen(mysocket, 1) < 0) {
                perror("error: listen");
                exit(1);
            }
            consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
        }
        else {
            addr = std::string(address);

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_addr.s_addr = inet_addr(address);
            dest.sin_port = htons(port);

            while(1) {
                consocket = socket(AF_INET, SOCK_STREAM, 0);

                if (connect(consocket, (struct sockaddr *)&dest, sizeof(struct sockaddr)) == 0) {
                    break;
                }

                close(consocket);
                usleep(1000);
            }
        }
        set_nodelay();
        stream = fdopen(consocket, "wb+");
        buffer = new char[NETWORK_BUFFER_SIZE];
        memset(buffer, 0, NETWORK_BUFFER_SIZE);
        setvbuf(stream, buffer, _IOFBF, NETWORK_BUFFER_SIZE);
        // setvbuf(stream, buffer, _IONBF, NETWORK_BUFFER_SIZE);
        std::cout << "connected" << std::endl;
    }

    ~NetIO(){
        fflush(stream);
        close(consocket);
        delete[] buffer;
    }

    void sync() {
        int tmp = 0;
        if(is_server) {
            send_data(&tmp, 1, true);
            recv_data(&tmp, 1, true);
        } else {
            recv_data(&tmp, 1, true);
            send_data(&tmp, 1, true);
            flush();
        }
    }

    void set_nodelay() {
        const int one=1;
        setsockopt(consocket,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
    }

    void set_delay() {
        const int zero = 0;
        setsockopt(consocket,IPPROTO_TCP,TCP_NODELAY,&zero,sizeof(zero));
    }

    void flush() {
        fflush(stream);
    }

    uint64_t get_total_comm() {
        return send_counter + recv_counter;
    }

    void send_data(const void* data, int len, bool buffered = true) {
        // If using non-buffered IO, make sure the bufferd IO stream is flushed
        if (!buffered) flush();
        send_counter += len;
        int sent = 0;
        int res;
        while(sent < len) {
            if (buffered) {
                res = fwrite(sent + (char*)data, 1, len - sent, stream);
            } else {
                res = send(consocket, sent + (char*)data, len - sent, 0);
            }
            if (res >= 0)
                sent+=res;
            else
                fprintf(stderr,"error: net_send_data %d\n", res);
        }
        has_sent = true;
    }

    int recv_data(void* data, int len, bool buffered = true) {
        if(has_sent) fflush(stream);
        has_sent = false;
        recv_counter += len;
        int sent = 0;
        int res;
        while(sent < len) {
            if (buffered) {
                res = fread(sent + (char*)data, 1, len - sent, stream);
            } else {
                res = recv(consocket, sent + (char*)data, len - sent, 0);
            }
            if (res >= 0)
                sent += res;
            else
                fprintf(stderr,"error: net_send_data %d\n", res);
        }
        return sent;
    }
};
}
#endif // NET_IO_H__
