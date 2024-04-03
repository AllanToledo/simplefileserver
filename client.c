#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define PORT            5555

#define T_MESSAGE 1
#define T_FILENAME 2
#define T_FILECONTENT 3

struct payload {
    char type;
    char hasMore;
    short size;
    char content[1020];
};

void write_to_server (int sockdes, char *filename) {
    ssize_t nbytes;
    struct payload body;
    memset(body.content, 0, sizeof(body.content));
    body.type = T_FILENAME;
    body.hasMore = 1;
    memcpy(body.content, filename, strlen(filename));
    nbytes = write(sockdes, &body, sizeof (body));
    if(nbytes < 0) {
        perror("write");
        printf("Error sending file name.\n");
        exit (EXIT_FAILURE);
    }
    FILE *file = fopen(filename, "rb");
    if(file == NULL) {
        printf("Error reading file.\n");
        perror("open file");
        return;
    }
    int byte;

    body.type = T_FILECONTENT;
    body.size = 0;

    while((byte = fgetc(file)) != EOF) {
        if(body.size == 1020) {
            body.hasMore = 1;
            nbytes = write(sockdes, &body, sizeof (body));
            //printf("%s\n", body.content);
            if (nbytes < 0) {
                perror ("write");
                exit (EXIT_FAILURE);
            }
            memset(body.content, 0, sizeof(body.content));
            body.size = 0;
        }
        body.content[body.size++] = (char) byte;
    }

    if(body.size > 0) {
        body.hasMore = 0;
        nbytes = write(sockdes, &body, sizeof (body));
        //printf("%s\n", body.content);
        if(nbytes < 0) {
            perror ("write");
            exit (EXIT_FAILURE);
        }
    }

    fclose(file);
}

void init_sockaddr(struct sockaddr_in *name, const char *hostname, uint16_t port){
    struct hostent *hostinfo;

    name->sin_family = AF_INET;
    name->sin_port = htons (port);
    hostinfo = gethostbyname (hostname);
    if(hostinfo == NULL) {
        fprintf (stderr, "Unknown host %s.\n", hostname);
        exit (EXIT_FAILURE);
    }
    name->sin_addr = *(struct in_addr *) hostinfo->h_addr;

}

int main (int argc, char *argv[]) {
    int sock;
    struct sockaddr_in servername;

    /* Create the socket. */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket (client)");
        exit(EXIT_FAILURE);
    }

    /* Connect to the server. */
    init_sockaddr(&servername, argv[1], PORT);
    if(0 > connect(sock, (struct sockaddr *) &servername, sizeof (servername))) {
        perror ("connect (client)");
        exit (EXIT_FAILURE);
    }

    write_to_server(sock, argv[2]);
    close(sock);
    exit(EXIT_SUCCESS);
}