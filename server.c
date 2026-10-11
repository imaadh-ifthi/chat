#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(void){
    printf("chat server starting...\n");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1){
        perror("socket");
        return 1;
    }
    printf("socket created: fd = %d\n", fd);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8001);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        return 1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
        perror("bind");
        close(fd);
        return 1;
    }
    printf("bound to port 8001\n");

    if (listen(fd, SOMAXCONN) == -1) {
        perror("listen");
        return 1;
    }
    printf("listening on port 8001, waiting for a client...\n");
    //pause();

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);

    int connfd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (connfd == -1){
        perror("accept");
        return 1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    printf("client connected from %s:%d\n", ip, ntohs(client_addr.sin_port));

    printf("accept returned connfd = %d\n", connfd);

    close(connfd);
    close(fd);
    
    return 0;
}