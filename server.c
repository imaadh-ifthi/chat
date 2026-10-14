#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define MAX_CLIENTS 10

typedef struct {
    int fd;
    int in_use; // 1 if live client, 0 if free
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;

int add_client(int fd){ //returns 1 if sucessfull, 0 if list is full
    int ok = 0;
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if (!clients[i].in_use){
            clients[i].fd = fd;
            clients[i].in_use = 1;
            ok = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return ok;

}

void remove_client(int fd){
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].in_use && clients[i].fd == fd){
            clients[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

void broadcast(int sender_fd, const char* msg, ssize_t len){
    pthread_mutex_lock(&clients_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].in_use && clients[i].fd != sender_fd){
            send(clients[i].fd, msg, len, 0);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

void *handle_client(void *arg){
    int fd = *(int *)arg;
    free(arg);
    printf("[thread %lu] client fd=%d joined\n", (unsigned long)pthread_self(), fd);

    printf("[thread %lu] handling client fd=%d\n", (unsigned long)pthread_self(), fd);
    printf("client connected, chatting...\n");

    char buf[1024];
    ssize_t n;

    while((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        broadcast(fd, buf, n);
    }

    if (n == 0){
        printf("client disconnected\n");
    } else {
        perror("recv");
    }

    remove_client(fd);
    printf("client fd=%d left\n", fd);

    close(fd);
    return NULL;

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

    while (1) {   
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd == -1){
            perror("accept");
            continue;
        }

        if(!add_client(connfd)){
            printf("Server full, rejecting fd=%d\n", connfd);
            close(connfd);
            continue;
        }

        int *pfd = malloc(sizeof(int));
        if(pfd == NULL){
            perror("malloc");
            remove_client(connfd);
            close(connfd);
            continue;
        }

        *pfd = connfd;

        pthread_t tid;
        int ret = pthread_create(&tid, NULL, handle_client, pfd);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
            remove_client(connfd);
            close(connfd);
            free(pfd);
            continue;
        }

        pthread_detach(tid);
        
    }
    close(fd);
    
    return 0;
}