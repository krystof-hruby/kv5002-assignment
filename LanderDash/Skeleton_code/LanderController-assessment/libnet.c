#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Gets address info
// Returns false if address info not found
int getaddr(const char *node, const char *service,
            struct addrinfo **address)
{
    struct addrinfo hints =
        {
            .ai_flags = 0,
            .ai_family = AF_INET,
            .ai_socktype = SOCK_DGRAM};

    if (node)
        hints.ai_flags = AI_ADDRCONFIG;
    else
        hints.ai_flags = AI_PASSIVE;

    int addr_error = getaddrinfo(node, service, &hints, address);

    if (addr_error)
    {
        fprintf(stderr, "Error trying to get address %s\n", gai_strerror(addr_error));
        return false;
    }

    return true;
}

// Creates an endpoint for communication using a socket
int mksocket(void)
{
    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socketfd == -1)
    {
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        return 0;
    }
    return socketfd;
}

// Binds socket to and address
int bindsocket(int sfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int err = bind(sfd, addr, addrlen);

    if (err == -1)
    {
        fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
        return false;
    }
    return true;
}

// Converts data from addr to URI host:port notation
char uri[80];
char *addrtouri(struct sockaddr *addr)
{
    struct sockaddr_in *a = (struct sockaddr_in *)addr;
    sprintf(uri, "%s:%d", inet_ntoa(a->sin_addr), ntohs(a->sin_port));
    return uri;
}

typedef size_t (*handler_t)(
    char *,
    size_t,
    char *, size_t,
    struct sockaddr_in *);

// Handles the server
int server(int srvrsock, handler_t handlemsg)
{
    const size_t buffsize = 4096; /* 4k */
    char message[buffsize], reply[buffsize];
    size_t msgsize, replysize;
    struct sockaddr clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    while (true)
    {
        // Receive message through socket
        msgsize = recvfrom(
            srvrsock,    /* server socket listening on */
            message,     /* buffer to put message */
            buffsize,    /* size of receiving buffer */
            0,           /* flags */
            &clientaddr, /* fill in with address of client */
            &addrlen);   /* number of bytes filled in */

        replysize = handlemsg(
            message,  /* incoming message */
            msgsize,  /* incoming message size */
            reply,    /* buffer for reply */
            buffsize, /* size of outgoing buffer */
            (struct sockaddr_in *)&clientaddr);

        if (replysize)
        {
            // Send message through socket to an address
            sendto(
                srvrsock,    /* server socket to use */
                reply,       /* outgoing message to send */
                replysize,   /* size of message */
                0,           /* flags */
                &clientaddr, /* address to send to */
                addrlen);    /* size of address structure */
        }
    }
}

void finished(int signal)
{
    exit(0);
}

int cleanupsock;

// Closes the socket
void cleanup(void)
{
    close(cleanupsock);
}