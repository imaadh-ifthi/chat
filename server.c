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
    char name[32];
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

void set_client_name(int fd, const char* name){
    pthread_mutex_lock(&clients_lock);
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].in_use && clients[i].fd == fd){
                strncpy(clients[i].name, name, sizeof(clients[i].name)-1);
                clients[i].name[sizeof(clients[i].name) -1] = '\0';
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

    char* ask_name = "Enter username: \n";
    send(fd, ask_name, strlen(ask_name), 0);
    
    char namebuf[32];
    ssize_t n_ = recv(fd, namebuf, sizeof(namebuf) - 1, 0);

    //catching clients that disconnect before sending a name
    if(n_ <= 0){
        remove_client(fd);
        close(fd);
        return NULL;
    }

    namebuf[n_] = '\0'; //null termination
    // remove trailing newline if present
    namebuf[strcspn(namebuf, "\r\n")] = '\0';

    //removing clients who have not inputted an username
    if (namebuf[0] == '\0') {
    const char *msg = "Empty username, goodbye.\n";
    send(fd, msg, strlen(msg), 0);
    remove_client(fd);
    close(fd);
    return NULL;
}

    set_client_name(fd, namebuf);

    char welcome[64];
    int wlen = snprintf(welcome, sizeof(welcome), "Welcome %s!\n", namebuf);
    send(fd, welcome, wlen, 0);

    //broadcasting user joined announcement
    char join_msg[64];
    int join_len = snprintf(join_msg, sizeof(join_msg), "*** %s joined ***\n", namebuf);
    broadcast(-1, join_msg, join_len);


    //broadcasting chat messages
    while((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        //check for /quit before the newline
        if (n >=5 && strncmp(buf, "/quit", 5) == 0 && (n == 5 || buf[5] == '\n' || buf[5] == '\r')){
            break; //disconnection
        }

        char outbuf[1100];
        int outlen = snprintf(outbuf, sizeof(outbuf), "%s: %.*s", namebuf, n, buf);
        broadcast(fd, outbuf, outlen);
    }

    if (n == 0){
        printf("client disconnected\n");
    } else {
        perror("recv");
    }

    remove_client(fd);
    char left_msg[64];
    int left_len = snprintf(left_msg, sizeof(left_msg), "*** %s left ***\n", namebuf);
    broadcast(-1, left_msg, left_len);
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