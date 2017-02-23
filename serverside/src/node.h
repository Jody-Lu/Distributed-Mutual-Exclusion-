#ifndef _node_h_
#define _node_h_

#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <mutex>
#include <chrono>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include "mythread.h"

#define MAX_NUM_NODES 5
#define PORT_START 55688
#define MAX_CS_ENTRY 40

using namespace std;

struct Connection
{
	int             sockDesc;
	sockaddr_in 	clientAddr;
	int             addrLen;
};

struct Message
{
	string   type;
	int 	 my_id;
	int 	 seq_no;

	Message(string s, int id, int n): type(s), my_id(id), seq_no(n) {}
	Message() {}
};

class Node {
private:
	int server_sock;
	int portno[MAX_NUM_NODES];
	struct sockaddr_in server_addr;

	static int myid;
	static int seq_no;
	static int num_message_send;
	static int num_message_recv;
	static int highest_seq_num;
	static int request_count, reply_count;
	static bool using_CS, waiting_CS;
	static bool all_nodes_connected;
	static bool received_all_reply;
	static bool exit_session;

	static int sockfd[MAX_NUM_NODES];
	static bool active_connection[MAX_NUM_NODES];
	static bool reply_from_node[MAX_NUM_NODES];
	static bool defer_node[MAX_NUM_NODES];
	static bool complete_node[MAX_NUM_NODES];
	static struct sockaddr_in serv_addr[MAX_NUM_NODES];

public:
	Node( int id );
	void nodeInit( );
	void SendRequestAndEnterCS( );
	void AcceptAndDistpatch( );
	static void *ProcessControlMessage( void *args );
	static void *ProcessCriticalSection( void *args );
	mutex _lock;
};

#endif