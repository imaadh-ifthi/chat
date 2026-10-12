#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

void sigchild_handler(int sig) {
    // reap all dead children
    while(waitpid(-1, NULL, WNOHANG) > 0){

    }
}

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

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchild_handler;
    sa.sa_flags = SA_RESTART; //restart interrupted syscalls
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGCHLD, &sa, NULL) == -1){
        perror("sigaction");
        return 1;
    }

    while (1) {   
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_len = sizeof(client_addr);

        int connfd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd == -1){
            perror("accept");
            continue;
        }

        pid_t pid = fork();

        if(pid == -1){
            perror("fork");
            close(connfd);
            continue;
        }

        if (pid == 0){
            close(fd);
            printf("[child %d] handling client\n", getpid());

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            
            printf("client connected from %s:%d\n", ip, ntohs(client_addr.sin_port));
            printf("accept returned connfd = %d\n", connfd);
            printf("client connected, echoing...\n");

            char buf[1024];
            ssize_t n;

            while((n = recv(connfd, buf, sizeof(buf), 0)) > 0) {
                send(connfd, buf, n, 0);
            }

            if (n == 0){
                printf("client disconnected\n");
            } else {
                perror("recv");
            }

            close(connfd);
            _exit(0);

        } else {
            close(connfd);
        }
        
    }
    close(fd);
    
    return 0;
}