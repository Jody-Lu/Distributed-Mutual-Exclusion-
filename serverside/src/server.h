#ifndef _server_h_
#define _server_h_

#include <iostream>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>

#include "mythread.h"
#include "client.h"

//#define PORT 55688

using namespace std;

/*
struct REQUEST {
    int seqNo;
    int nodeNo;
};
*/

class Server {

  private:
    static vector<Client> clients;

    // Socket stuff
    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    char buff[256];

  public:
    Server(int portno);
    void AcceptAndDispatch();
    static void * HandleClient(void *args);

  private:
    static void ListClients();
    static void SendToAll(char *message);
    static int FindClientIndex(Client *c);
    static void SendRequestToAll();
};

#endif
