/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>   // system call types
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define NUM_THREADS 5


/* Called when a system call fails */
void error( const char *msg )
{
    perror(msg);
    exit(1);
}

struct Test {
    int height;
    int weight;
};

/******** DOSTUFF() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff( int newsockfd )
{
    int n;
    char buffer[256];

    bzero( buffer, 256 );
    n = read( newsockfd, buffer, 256 );

    if ( n < 0 )
    {
        error("ERROR reading from socket");
    }

    printf("Here is the message: %s\n", buffer);

    n = write(newsockfd, "I got your message", 18);
    if ( n < 0 )
    {
        error("ERROR writing to socket");
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    //char buffer[256]; // The server reads characters from the socket connection into this buffer.
    struct sockaddr_in serv_addr, cli_addr;
    //int n;

    // Check user input argument
    if ( argc < 2 ) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    // Create the socket
    /*
        The socket() function creates a new socket.
        It takes 3 arguments,

            a. AF_INET: address domain of the socket.
            b. SOCK_STREAM: Type of socket. a stream socket in
            which characters are read in a continuous stream (TCP)
            c. Third is a protocol argument: should always be 0. The
            OS will choose the most appropiate protocol.

            This will return a small integer and is used for all
            references to this socket. If the socket call fails,
            it returns -1.

    */
    sockfd = socket( AF_INET, SOCK_STREAM, 0 );

    if ( sockfd < 0 )
    {
        error("ERROR opening socket");
    }

    // Set serv_addr to zero
    bzero( (char *) &serv_addr, sizeof(serv_addr) );

    // Set portno
    portno = atoi(argv[1]);

    // Set serv_addr attributes
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Call the bind() system call
    /*
        The bind() system call binds a socket to an address,
        in this case the address of the current host and port number
        on which the server will run. It takes three arguments,
        the socket file descriptor. The second argument is a pointer
        to a structure of type sockaddr, this must be cast to
        the correct type.
    */
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
        sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }

    // listen on the process for connections
    /*
        The listen system call allows the process to listen
        on the socket for connections.

        The program will be stay idle here if there are no
        incomming connections.

        The first argument is the socket file descriptor,
        and the second is the size for the number of clients
        i.e the number of connections that the server can
        handle while the process is handling a particular
        connection. The maximum size permitted by most
        systems is 5.

    */

    /*
        The accept() system call causes the process to block
        until a client connects to the server. Thus, it wakes
        up the process when a connection from a client has been
        successfully established. It returns a new file descriptor,
        and all communication on this connection should be done
        using the new file descriptor. The second argument is a
        reference pointer to the address of the client on the other
        end of the connection, and the third argument is the size
        of this structure.
    */

    listen(sockfd, 5);
    pthread_t threads[NUM_THREADS];
    int rc;
    int counter = 0;

    clilen = sizeof(cli_addr);

    // connect to new client with socket descripter - newsockfd
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if ( newsockfd < 0 )
    {
        error( "ERROR on accept" );
    }

    while ( 1 )
    {
        int n;
        char buffer[256];

        bzero( buffer, 256 );
        n = read( newsockfd, buffer, 256 );

        if ( n < 0 )
        {
            error("ERROR reading from socket");
        }

        printf("Here is the message: %s\n", buffer);

        n = write(newsockfd, "I got your message\n", 18);
        if ( n < 0 )
        {
            error("ERROR writing to socket");
        }
    }

    close( sockfd );
    pthread_exit(NULL);

    return 0;
}
