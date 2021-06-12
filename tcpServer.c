
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>

// use linked list instead of array

// file ptr for logs
FILE *logptr;

// define constants for use later
#define BUF_SIZE 500
#define BACKLOG 10 /* Passed to listen() */
#define MAX_CONNECTIONS 100

// struct to hold client data
struct connectionnamepair
{
    int *socket;
    char name[50];
};

// global array and length of it for use in server
struct connectionnamepair connections[MAX_CONNECTIONS];
int numConnections = 0;

// sends specific mode of message to all clients, used to send general message, as well as client list
// mode 2 is a remnant of a previous system
void sendToAll(char *buf, int mode) // mode 1 == message, 2 == remove from list, 3 == client log
{
    // tmpbuf to store string
    char tmpbuf[BUF_SIZE];
    if (mode == 1)
    {
        sprintf(tmpbuf, "m%s", buf);
    }
    else if (mode == 2)
    {
        sprintf(tmpbuf, "r%s", buf);
    }
    else if (mode == 3)
    {
        sprintf(tmpbuf, "c%s", buf);
    }
    // loop through connections and send to everyone
    for (int i = 0; i < numConnections; i++)
    {

        if (!send(*connections[i].socket, tmpbuf, sizeof(tmpbuf), 0))
        {
            fprintf(stderr, "Error sending response\n");
        }
    }
    fprintf(logptr, "Send to all: %s\n", tmpbuf);
    fflush(logptr);
}

// function called for each connection in a pthread
void *handle(void *con)
{
    /* send(), recv(), close() */
    char buf[BUF_SIZE];

    // since passed as void * need to conver it to a struct we can use
    struct connectionnamepair *connection = (struct connectionnamepair *)con;

    // block for receive and fill buf with the received data when received
    if (!recv(*connection->socket, buf, sizeof(buf), 0))
    {
        perror("receive");
    }

    // move data into name for later
    strcpy(connection->name, buf);
    // log received name
    fprintf(logptr, "Received Name for Client: %s\n", connection->name);

    // send acknowledgeement and setup of name
    if (!send(*connection->socket, buf, sizeof(buf), 0))
    {
        fprintf(stderr, "Error sending response\n");
    }

    //sendToAll(buf, 3);

    // list all logged in Users to the newly connected client
    char tmpbuf[BUF_SIZE];

    sprintf(tmpbuf, "w_______________________");
    if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
    {
        fprintf(stderr, "Error sending response\n");
    }
    sprintf(tmpbuf, "wLogged In Clients:");
    if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
    {
        fprintf(stderr, "Error sending response\n");
    }
    for (int i = 0; i < numConnections; i++)
    {
        sprintf(buf, "c%s", connections[i].name);
        if (*connections[i].socket != -1)
        {
            //fprintf(stderr, "existing username %d: %s\n", i, buf);
            if (!send(*connection->socket, buf, sizeof(buf), 0))
            {
                fprintf(stderr, "Error sending response\n");
            }
        }
    }
    sprintf(tmpbuf, "w_______________________\n");
    if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
    {
        fprintf(stderr, "Error sending response\n");
    }

    // loop endlessly to handle incoming message from this client, this will exist either,
    // when the client program closes, or we receive "exit" or "exit\n" (handling both due to some weird behaviour)
    while (1)
    {
        // 0 is orderly shutdown so clean if 0
        // if (recv(*connection->socket, buf, sizeof(buf), 0))
        // {
        //     perror("receive");
        // }

        // send msg to everyone to remove here prob
        // recv each byte and build string until exit/end character then go and come back after

        // handle disconnections and remove from active connection list
        if (!recv(*connection->socket, buf, sizeof(buf), 0) || strcmp(buf, "exit") == 0 || strcmp(buf, "exit\n") == 0)
        {
            //sendToAll(connection->name, 2);
            printf("%s disconnected...\n", connection->name);
            for (int i = 0; i < numConnections; i++)
            {
                if (*connection->socket == *connections[i].socket)
                {
                    close(*connection->socket);

                    *connections[i].socket = -1;

                    return 0;
                }
            }
        }
        // zero tmp buf and setup for handling commands and messages
        bzero(tmpbuf, BUF_SIZE);
        char msgbuf[BUF_SIZE];
        strcpy(msgbuf, buf);
        printf("%s\n", buf);

        // if first char is / we are tryint to do a command
        // /w and /u are only valid, handling whispers and listing userlist
        // /w <loggedinclient> <message>
        // /u
        if (buf[0] == '/')
        {
            strtok(buf, " ");

            if (buf[1] == 'w')
            {
                char *name = strtok(NULL, " ");
                bool foundit = 0;
                char tmpbuf[BUF_SIZE];
                for (int i = 0; i < numConnections; i++)
                {
                    // if user is found go through and format strings and send to the recipient and sender their
                    // message (either To or From), else tell them its not a valid client
                    if (strcmp(connections[i].name, name) == 0 && *connections[i].socket != -1)
                    {
                        char *msg;
                        char *s;
                        fprintf(stderr, "msgbuf: %s\n", msgbuf);
                        s = strstr(msgbuf, name);
                        fprintf(stderr, "name: %s\n", name);
                        fprintf(stderr, "s: %s\n", s);
                        msg = s + strlen(name);
                        sprintf(tmpbuf, "w(Whisper From %s)%s", connection->name, msg);
                        fprintf(stderr, "from whisper buf: %s\n", tmpbuf);

                        if (!send(*connections[i].socket, tmpbuf, sizeof(tmpbuf), 0))
                        {
                            fprintf(stderr, "Error sending response\n");
                        }
                        sprintf(tmpbuf, "w(Whisper To %s)%s", name, msg);
                        fprintf(stderr, "to whisper buf: %s\n", tmpbuf);

                        if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
                        {
                            fprintf(stderr, "Error sending response\n");
                        }
                        foundit = 1;
                    }
                }
                if (!foundit)
                {
                    // probably need to make seperate error thingie in client to handle
                    // stuff like this
                    sprintf(tmpbuf, "w%s is not a valid user", name);

                    if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
                    {
                        fprintf(stderr, "Error sending response\n");
                    }
                }
            }
            // loop through and send the client list with formatted output
            else if (buf[1] == 'u')
            {
                sprintf(tmpbuf, "w_______________________");
                if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
                {
                    fprintf(stderr, "Error sending response\n");
                }
                sprintf(tmpbuf, "wLogged In Clients:");
                if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
                {
                    fprintf(stderr, "Error sending response\n");
                }
                for (int i = 0; i < numConnections; i++)
                {
                    sprintf(buf, "c%s", connections[i].name);
                    if (*connections[i].socket != -1)
                    {
                        //fprintf(stderr, "existing username %d: %s\n", i, buf);
                        if (!send(*connection->socket, buf, sizeof(buf), 0))
                        {
                            fprintf(stderr, "Error sending response\n");
                        }
                    }
                }
                sprintf(tmpbuf, "w_______________________\n");
                if (!send(*connection->socket, tmpbuf, sizeof(tmpbuf), 0))
                {
                    fprintf(stderr, "Error sending response\n");
                }
            }
            // else if (buf[1] == 'v')
            // {
            //     fprintf(stderr, "got a audio frame in theory\n");
            //     fprintf(stderr, "%s", buf);
            // }
            // invalid command tell teh cleint
            else
            {
                char nope[] = "mInvalid Command!";
                printf("%s\n", buf);
                if (!send(*connection->socket, nope, sizeof(nope), 0))
                {
                    fprintf(stderr, "Error sending response\n");
                }
            }
        }
        else
        {
            sprintf(tmpbuf, "%s_%s", buf, connection->name);
            sendToAll(tmpbuf, 1);
        }
        fprintf(logptr, "Sent Message: %s\n", tmpbuf);
        fflush(logptr);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    // setup logptr
    logptr = fopen("ChatServer.log", "a");

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    pthread_t thread;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    int newsocket;
    char buf[BUF_SIZE];

    // check args are valid
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP address */
    hints.ai_protocol = 0;           /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    fprintf(logptr, "\n\nServer Starting\n");
    // setup hints and result for later connections
    // also error check getaddrinfo as if this fails we have a major network problem
    s = getaddrinfo(NULL, argv[1], &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully bind(2).
              If socket(2) (or bind(2)) fails, we (close the socket
              and) try the next address. */
    // look through results until we bind or completely fail and exit with exit_failure
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        close(sfd);
    }

    freeaddrinfo(result); /* No longer needed */

    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }
    // setup server to listen, with maximum 10 people waiting to connect
    if (listen(sfd, BACKLOG) == -1)
    {
        perror("listen");
        return 0;
    }
    fprintf(logptr, "Server Started and Listening\n");

    // loop to handle clients connecting
    while (1)
    {
        peer_addr_len = sizeof(peer_addr);
        // wait for client connection
        newsocket = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        fprintf(logptr, "Server Accepted Client\n");

        if (newsocket == -1)
        {
            perror("accept");
            continue; /* Ignore failed request */
        }

        // setup int pointer for handle thread
        int *safesock = (int *)(malloc(sizeof(int)));
        if (safesock)
        {
            *safesock = newsocket;
            connections[numConnections].socket = safesock;

            // startup pthread with handle passing the connection
            if (pthread_create(&thread, NULL, handle, (void *)&connections[numConnections++]))
            {
                fprintf(stderr, "Failed to create thread\n");
            }
            printf("numConnections: %d\n", numConnections);
            fprintf(logptr, "numConnections: %d\n", numConnections);
        }
        else
        {
            perror("malloc");
        }
        // flush the log ptr before loop again
        fflush(logptr);
    }
}