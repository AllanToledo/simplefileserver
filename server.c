#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT    5555
#define MAXMSG  512

#define T_MESSAGE 1
#define T_FILENAME 2
#define T_FILECONTENT 3

struct payload {
    char type;
    char hasMore;
    short size;
    char content[1020];
};

int make_socket (uint16_t port) {
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &name, sizeof (name)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    return sock;
}

int read_from_client (int sockdes, FILE **file_received){
    struct payload received_payload;
    ssize_t nbytes;

    nbytes = read(sockdes, &received_payload, sizeof (received_payload));

    if (nbytes < 0){
        /* Read error. */
        perror ("read");
        exit (EXIT_FAILURE);
    } else if (nbytes == 0)
        /* End-of-file. */
        return -1;
    else {

        switch(received_payload.type) {
            case T_FILENAME: {
                char filename[FILENAME_MAX];
                snprintf(filename, FILENAME_MAX, "./received/%s", received_payload.content);
                printf("File: %s\n", filename);
                *file_received = fopen(filename, "wb");
                FILE *file = *file_received;
                if(file == NULL) {
                    printf("File cannot be open.\n");
                    return -1;
                }
            } break;

            case T_FILECONTENT: {
                FILE *file = *file_received;
                fwrite(received_payload.content, received_payload.size, 1, file);
            } break;
        }

        return 0;
    }
}

int main() {

    char dirname[FILENAME_MAX];
    getcwd(dirname, FILENAME_MAX);

    printf("Working directory:\n%s\n", dirname);
    printf("Creating \"received\" directory.\n");

    if(mkdir("received", S_IRWXU | S_IRWXG | S_IRWXO)) {
        if(errno == EEXIST) {
            printf("Directory already created.\n");
        } else {
            printf("Error %d in creating directory.\n", errno);
            exit(errno);
        }
    } else {
        printf("Successfully created directory.\n");
    }

    int sock = make_socket(PORT);
    if(listen(sock, 1) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    fd_set active_fd_set, read_fd_set;
    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    while(1) {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0){
            perror("select");
            exit(EXIT_FAILURE);
        }

        FILE *file_received;

        /* Service all the sockets with input pending. */
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET (i, &read_fd_set)) {
                if (i == sock){
                    /* Connection request on original socket. */
                    int new;
                    struct sockaddr_in clientname;
                    size_t size = sizeof (clientname);
                    new = accept(sock, (struct sockaddr *) &clientname, (socklen_t *) &size);
                    if(new < 0) {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    fprintf(stderr, "Server: connect from host %s, port %hu.\n",
                            inet_ntoa(clientname.sin_addr),
                             ntohs(clientname.sin_port));
                    FD_SET(new, &active_fd_set);
                } else {
                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i, &file_received) < 0) {
                        if(file_received > 0) {
                            fclose(file_received);
                            file_received = 0;
                        }
                        close(i);
                        FD_CLR(i, &active_fd_set);
                    }
                }
            }
        }
    }




    printf("Programs runs ok.\n");
    return 0;
}
