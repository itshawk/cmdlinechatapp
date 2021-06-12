#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

// setup constant and global
#define BUF_SIZE 500
int sfd;

// predefine sender
void sender(char *in);

// handle ctrl-c to send exit close program
void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        //fprintf(stderr, "\033[A\33[2K\r");
        fprintf(stderr, "\33[2K\rClosed.\n");
        exit(EXIT_SUCCESS);
        char *s = "exit\n";
        sender(s);
    }
}

// sender function called from the main thread when client enters something
void sender(char *in)
{
    int len;
    // add terminating bit at the end instead of \n
    in[strlen(in) - 1] = '\0';
    len = strlen(in) + 1;
    /* +1 for terminating nullptrptr byte */

    // if its too long give error
    if (len > BUF_SIZE)
    {
        fprintf(stderr,
                "Ignoring long message in argument\n");
    }
    // write to the socket
    if (write(sfd, in, len) != len)
    {
        perror("write");
    }
    // if we are exiting then exit with exit_success
    if (strcmp(in, "exit\n") == 0)
    {
        exit(EXIT_SUCCESS);
    }
    // clear line of input for cleanliness for client
    fprintf(stderr, "\033[A\33[2K\r");
}

// receiver function running in its own thread
void *receiver()
{
    char buf[BUF_SIZE];
    ssize_t nread;
    // setup bool for first time username setup
    bool setup = 0;

    // start loop
    while (1)
    {
        // zero buf
        bzero(buf, BUF_SIZE);
        // block and wait for receive
        nread = recv(sfd, buf, sizeof(buf), 0);
        //printf("buf: %s\n", buf);
        if (nread == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        // 0 bytes returned on tcp means server shutdown orderly
        if (nread == 0)
        {
            perror("Intended Server Shutdown");

            exit(EXIT_SUCCESS);
        }
        // username output to cmd line
        if (!setup)
        {

            fprintf(stderr, "Logged in as: %s\n", buf);
            //fprintf(stderr, "Currently Logged In Users:\n");
            setup = 1;
        }
        // we did username onto messaging and commands
        // everything we get from server is preformatted in a way that the belwo
        // handles it quickly and nicely
        else
        {
            //printf("%c\n", buf[0]);
            if (buf[0] == 'm')
            {
                //fprintf(stderr, "%s", buf);
                char *tmpbuf = strchr(buf, 'm') + 1;
                //printf("tmpbuf_m: %s\n", tmpbuf);
                char *msg = strtok(tmpbuf, "_");
                char *name = strtok(NULL, "_");
                fprintf(stderr, "%s: %s\n", name, msg);
                //fflush(stdout);
            }
            else if (buf[0] == 'w')
            {
                char *msg = strchr(buf, 'w') + 1;
                //printf("msg: %s\n", msg);

                fprintf(stderr, "%s\n", msg);
                //printf("%s\n", msg);
            }
            else if (buf[0] == 'c')
            {
                //printf("msg: %s\n", buf);
                char *msg = strchr(buf, 'c') + 1;

                fprintf(stderr, "%s\n", msg);
            }
            // old system remnant
            else if (buf[0] == 'r')
            {
                //printf("msg: %s\n", buf);

                fprintf(stderr, "%s\n", buf);
            }
        }
    }
}

int main(int argc, char **argv)
{
    // setup up internal program signal handling (i.e. SIGINT)
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    int len;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s host port...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Obtain address(es) matching host/port. */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* tcp */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */
    // setup hints and result for later connections
    // also error check getaddrinfo as if this fails we have a major network problem
    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0)
    {
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        gai_strerror(s);
        exit(EXIT_FAILURE);
    }
    // look through results until we bind or completely fail and exit with exit_failure

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            break; /* Success */
        }

        close(sfd);
    }

    freeaddrinfo(result); /* No longer needed */

    if (rp == NULL)
    { /* No address succeeded */
        perror("No address succeeded");
        exit(EXIT_FAILURE);
    }
    // wait for client to send something and clear the line
    // send the content to server
    char msg[200];
    bool first = true;
    fprintf(stderr, "Enter your Client Name as the first msg:\n");
    while (fgets(msg, 200, stdin) != NULL)
    {
        len = strlen(msg) + 1;
        // if no content only \n + 1
        if (!(len - 2))
        {
            fprintf(stderr, "\033[A\33[2K\r");

            continue;
        }
        /* +1 for terminating null byte */

        if (len > BUF_SIZE)
        {
            fprintf(stderr,
                    "Ignoring long message in argument\n");
            continue;
        }

        sender(msg);
        // after we send our username quickly spawn the receiver thread.
        if (first)
        {
            pthread_t thread;
            if (pthread_create(&thread, NULL, receiver, NULL))
            {
                fprintf(stderr, "Failed to create thread\n");
            }
            first = false;
        }
    }
}