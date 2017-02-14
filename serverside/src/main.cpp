#include <iostream>
#include <string>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>

#include "mythread.h"
#include "server.h"

#define MAX_NUM_NODES 2
#define PORT_START 55688
#define MAX_CS_ENTRY 40

using namespace std;


// global variables
pthread_t csThread;   // thread for request critical section of this node
pthread_t connThread; // thread for handle connection to this node

int sockfd[MAX_NUM_NODES]; // each sockfd for connecting to each node
int portno[MAX_NUM_NODES]; // port numbers of nodess

bool activeConnection[MAX_NUM_NODES]; 			 // whether connecting to server successfully
struct sockaddr_in serv_addr[MAX_NUM_NODES]; // server address of other nodes
bool reply_from_node[MAX_NUM_NODES]; 				 // check which node has reply
bool defer_node[MAX_NUM_NODES];							 // record which node is defered
bool completeNode[MAX_NUM_NODES];						 // whether node i has completed

int seqNo;        		// sequence number of the message
int serverSock;				// socket of this node
int myid;         		// id of this node
int num_message_send; // messages sent by this node
int num_message_recv; // messages received by this node
int no_cs_entry;			// times of entering CS
int highestSeqNum;

bool usingCS;							// whether this node is using CS
bool waitingCS;						// whether this node is waiting CS
bool all_nodes_connected; // whether all nodes are connected with each other
bool receivedAllReply;		// whether receiving all replies
bool exitSession;

struct sockaddr_in serverAddr; // address of this node

// mutex
pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

// Function declared
int generateRandomeNumber(int min, int max);



typedef struct
{
	int 					sockDesc;
	sockaddr_in 	clientAddr;
	int 					addrLen;
}Connection;

struct Message
{
	string type;
	int 	 my_id;
	int 	 seqNo;

	Message(string s, int id, int n): type(s), my_id(id), seqNo(n) {}
	Message() {}
};

string messageSerialization(Message m);
Message messageDeserialization(char *s);


string messageSerialization(Message m)
{
	// type,my_id,seqNo
	string res = "";
	res += ( m.type + "," + to_string(m.my_id) + "," + to_string(m.seqNo) );
	return res;
}

Message messageDeserialization(char *s)
{
	Message m;
	string ss(s);
	int pos = 0, begin = 0;

	pos = ss.find_first_of( ",", begin );
	m.type = ss.substr( begin, pos);

	begin = pos + 1;

	pos = ss.find_first_of( ",", begin );
	m.my_id = atoi( ss.substr( begin, pos ).c_str() );

	begin = pos + 1;

	pos = ss.find_first_of( ",", begin );
	m.seqNo = atoi( ss.substr( begin, pos ).c_str() );

	printf("type: %s, id: %d, seqNo: %d\n", m.type.c_str(), m.my_id, m.seqNo);

	return m;
}

void initializationGlobalData(int id)
{
	// When initialization, try to connect to each node

	hostent* host;

	// Iniitalize myid
	myid 						 		= id;
	seqNo 					 		= 0;
	num_message_send 		= 0;
	num_message_recv 		= 0;
	no_cs_entry 		 		= 0;
	highestSeqNum				= 0;
	usingCS 			   		= false;
	waitingCS        		= false;
	all_nodes_connected = false;
	receivedAllReply 	  = false;
	exitSession					= false;

	for ( int i = 0; i < MAX_NUM_NODES; i++ )
	{
		/* Initialize active connection */
		activeConnection[i] = false;
		reply_from_node[i]  = false;
		defer_node[i] 			= false;
		completeNode[i]			= false;

		/* Initialize port number */
		portno[i] = PORT_START + i;

		if ( i != myid )
		{
			/* Initialize sockfd */
			sockfd[i] = socket(AF_INET, SOCK_STREAM, 0);

			if ( sockfd[i] < 0 )
			{
				cerr << "ERROR opening socket" << endl;
			}

			/* Initialize serv_addr */
			memset(&serv_addr[i], 0, sizeof(sockaddr_in));
			serv_addr[i].sin_family = AF_INET;
			serv_addr[i].sin_port   = htons(portno[i]);

			string hostname = "dc0" + to_string(i + 1) + ".utdallas.edu";
			host = gethostbyname( hostname.c_str() );
			//host = gethostbyname( "127.0.0.1" );
			if (host == NULL)
			{
				cout << "ERROR host" << endl;
			}
			else
			{
				memcpy(&serv_addr[i].sin_addr, host->h_addr_list[0], host->h_length);
			}

			if ( connect(sockfd[i], (struct sockaddr *) &serv_addr[i], sizeof(serv_addr[i])) == -1 )
			{
				cout << "node " << i << " doesn't launch yet..." << endl;
			}
			else
			{
				activeConnection[i] = true;
			}
		}
	}
	return;
}

/*
 Connect to all nodes
 wait
 send message
 receive message
 enter CS
 reply
*/
void *ProcessCriticalSection(void *args)
{

	int time_to_wait = 0;

	/* Connect to all other nodes */
	pthread_mutex_lock( &dataMutex );

	for ( int i = 0; i < MAX_NUM_NODES; i++ )
	{
		if ( i == myid )
		{
			continue;
		}

		while ( activeConnection[i] == false )
		{
			sockfd[i] = socket( AF_INET, SOCK_STREAM, 0 );

			if ( connect( sockfd[i], (struct sockaddr *) &serv_addr[i], sizeof(serv_addr[i]) ) > -1 )
			{
				cout << "Successfully connecting to node: " << i << endl;
				activeConnection[i] = true;
			}
			else
			{
				close(sockfd[i]);
			}
		}
	}
	all_nodes_connected = true;

	pthread_mutex_unlock( &dataMutex );

	while ( no_cs_entry < MAX_CS_ENTRY )
	{
		/* Wait for a period of time */
		pthread_mutex_lock( &dataMutex );

		if ( myid % 2 == 0 ) // the even numbered node
		{
			pthread_mutex_unlock( &dataMutex );
			time_to_wait = generateRandomeNumber( 5, 10 );
			usleep( 10000 * time_to_wait );
		}
		else // the odd numbered node
		{
			pthread_mutex_unlock( &dataMutex );
			if ( no_cs_entry > 20 )
			{
				time_to_wait = generateRandomeNumber( 5, 10 );
				usleep( 10000 * time_to_wait );
			}
			else
			{
				time_to_wait = generateRandomeNumber( 45, 50 );
				usleep( 10000 * time_to_wait );
			}
		}

		/* Send REQUEST */
		pthread_mutex_lock( &dataMutex );
		printf( "Send REQUEST and wait CS\n" );

		waitingCS = true;
		seqNo = highestSeqNum + 1;

		Message m("REQUEST", myid, seqNo);
		string mm = messageSerialization( m );

		for ( int i = 0; i < MAX_NUM_NODES; i++ )
		{
			/* Send REQUEST only when there is no REPLY. */
			if ( i != myid && !reply_from_node[i] )
			{
				num_message_send++;
				send( sockfd[i], mm.c_str(), strlen(mm.c_str()), 0 );
			}
		}

		pthread_mutex_unlock( &dataMutex );

		/* After sending REQUESTs, wait until receiving all REPLYs */
		printf("Entering CS\n");
		while ( 1 )
		{
			pthread_mutex_lock( &dataMutex );

			if ( receivedAllReply )
			{
				usingCS = true;
				waitingCS = false;
				no_cs_entry++;

				printf( "entering\n" );
				usleep( 30000 );
				pthread_mutex_unlock( &dataMutex );
				break;
			}
			else
			{
				pthread_mutex_unlock( &dataMutex );
			}
		}

		/* After entering CS, send REPLYs to nodes in defer_node */
		printf("Sending REPLY\n");
		pthread_mutex_lock( &dataMutex );
		usingCS = false;

		Message r("REPLY", myid, 0);

		string rr = messageSerialization( r );

		for ( int i = 0; i < MAX_NUM_NODES; i++ )
		{
			/* reply to all nodes that was defered. */
			if ( i != myid && defer_node[i] )
			{
				defer_node[i] 		 = false;
				reply_from_node[i] = false;
				receivedAllReply   = false;

				send( sockfd[i], rr.c_str(), strlen(rr.c_str()), 0 );
			}
		}
		pthread_mutex_unlock( &dataMutex );
	}

	pthread_mutex_lock( &dataMutex );

	if ( myid != 0 )
	{
		Message complete( "COMPLETE", myid, 0 );
		string cc = messageSerialization( complete );

		send( sockfd[0], cc.c_str(), strlen(cc.c_str()) + 1, 0 );
	}
	else
	{
		completeNode[0] = true;
	}

	pthread_mutex_unlock( &dataMutex );

	printf("Exit Session (Process Critical Section)\n");
	pthread_exit( 0 );
}

// Process messages sent to this node
// 1. REPLY
// 2. COMPLETE
// 3. ERQUEST

void *ProcessControlMessage(void *args)
{

	int numBytesRead = 0;
	int numBytesSend = 0;
	int count_reply  = 0;
	int counter 		 = 0;
	bool myPriority  = false;
	bool exitCompute = false;

	if ( args == NULL )
	{
		pthread_exit(0);
	}

	Connection *conn = (Connection *)args;

	//printf( "(PCM) conn->Desc: %d\n", conn->sockDesc );

	while ( 1 )
	{
		Message m;
		char buffer[256];
		//string mm;
		numBytesRead = recv( conn->sockDesc, buffer, 256, 0 );
		m = messageDeserialization( buffer );

		printf( "Read %d Bytes\n", numBytesRead);
		// Here
		printf( "Read %d bytes, type: %s, from node: %d seqNo: %d\n", numBytesRead, m.type.c_str(), m.my_id, m.seqNo);

		if ( numBytesRead == 0 )
		{
			break;
		}

		//printf( "Read %d bytes, type: %s, from node: %d seqNo: %d\n", numBytesRead, m.type.c_str(), m.my_id, m.seqNo);

		if ( m.type == "REQUEST" )
		{
			pthread_mutex_lock( &dataMutex );
			while ( 1 )
			{
				if ( all_nodes_connected )
				{
					break;
				}
			}

			/* Record the highest comming seqNo */
			if ( m.seqNo > highestSeqNum )
			{
				highestSeqNum = m.seqNo;
			}

			num_message_recv++;

			/* Two senario that this node gets priority
			 - my.seqNo > seqNo
			 - m.seqNo == seqNo && m.my_id > myid
			 */
			if ( m.seqNo > seqNo || ( m.seqNo == seqNo && m.my_id > myid ) )
			{
				myPriority = true;
			}

			if ( usingCS || ( waitingCS && myPriority ) )
			{
				/*
				If this node is using CS or waiting CS and got priority,
				then defering the comming REQUEST.
				 */
				defer_node[m.my_id] = true;
				printf( "Case 1: Defer comming REQUEST.\n" );

			}
			else if ( ( !usingCS || !waitingCS ) || ( waitingCS && !reply_from_node[m.my_id] && !myPriority ) )
			{
				/*
				If this node is not using CS or not waiting CS, or waiting CS but not receiving REPLY from m.my_id
				and not its priority, then, send REPLY to m.my_id.
				 */

				reply_from_node[m.my_id] = false;
				receivedAllReply = false;

				Message rpy("REPLY", myid, 0);
				string rr = messageSerialization( rpy );

				send( sockfd[m.my_id], rr.c_str(), strlen(rr.c_str()), 0 );
				printf( "Case2: Send REPLY message to node.\n" );

			}
			else if ( waitingCS && reply_from_node[m.my_id] && !myPriority )
			{
				/*
				If this node is waiting CS and got REPLY from m.my_id but not its priority,
				then, send REPLY to m.ny_id.
				 */

				reply_from_node[m.my_id] = false;
				receivedAllReply = false;

				Message rpy("REPLY", myid, 0);
				string rr = messageSerialization( rpy );

				send( sockfd[m.my_id], &rr, sizeof(rr), 0 );
				printf( "Case3: Send REPLY message to node.\n" );

				/*
				usleep( 10000 );

				Message r( "REQUEST", myid, seqNo );
				send( sockfd[m.my_id], &r, sizeof(r), 0 );
				*/
			}
			pthread_mutex_unlock( &dataMutex );
		}
		else if ( m.type == "REPLY" )
		{
			counter = 0;
			pthread_mutex_lock( &dataMutex );
			num_message_recv++;
			count_reply++;
			reply_from_node[m.my_id] = true;

			if ( !receivedAllReply )
			{
				for ( int i = 0; i < MAX_NUM_NODES; i++ )
				{
					if ( i != myid && reply_from_node[i] )
					{
						counter++;
					}
				}

				if ( counter == MAX_NUM_NODES - 1 )
				{
					receivedAllReply = true;
				}
			}
			pthread_mutex_unlock( &dataMutex );
		}
		else if ( m.type == "COMPLETE" )
		{
			pthread_mutex_lock( &dataMutex );
			if ( myid == 0 )
			{
				exitCompute = true;
				completeNode[m.my_id] = true;

				for ( int i = 0; i < MAX_NUM_NODES; i++ )
				{
					if ( !completeNode[i] )
					{
						exitCompute = false;
						break;
					}
				}
				if ( exitCompute )
				{
					//printf("Exit compute is true!\n");

					exitSession = true;

					Message c( "COMPLETE", myid, 0 );
					string cc = messageSerialization( c );

					for ( int i = 0; i < MAX_NUM_NODES; i++ )
					{
						if ( i != myid )
						{
							send( sockfd[i], cc.c_str(), strlen(cc.c_str()), 0 );
						}
					}
				}
			}
			else if (m.my_id == 0)
			{
				exitSession = true;
			}
			pthread_mutex_unlock( &dataMutex );
		}

		pthread_mutex_lock( &dataMutex );
		if ( exitSession )
		{
			//printf("Close socket connections\n");
			for ( int i = 0; i < MAX_NUM_NODES; i++ )
			{
				if ( i != myid )
				{
					close( sockfd[i] );
				}
			}

			for ( int i = 0; i < MAX_NUM_NODES; i++ )
			{
				printf( "%d", completeNode[i] );
			}
			printf( "\n" );
			printf("e: %d\n", exitSession);

			pthread_mutex_unlock( &dataMutex );
			break;
		}
		pthread_mutex_unlock( &dataMutex );
	}
	printf("Exit Session (Process Control)\n");
	Connection *con = (Connection *)args;
	close( con->sockDesc );
	free( con );
	pthread_exit( 0 );
}


// Generate random number between a and b
int generateRandomeNumber(int min, int max)
{
	return rand() % min + max;
}

int main(int argc, char const *argv[])
{

	Connection *conn;

	initializationGlobalData( atoi(argv[1]) );

  serverSock = socket( AF_INET, SOCK_STREAM, 0 );

  if ( serverSock <= 0 )
  {
  	cout << " ERROR creating server" << endl;
  	return -1;
  }

  memset( &serverAddr, 0, sizeof(sockaddr_in) );
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(portno[myid]);

  int yes = 1;

  // Avoid bind error if the socket was not close()'d last time;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if ( bind(serverSock, (struct sockaddr *) &serverAddr, sizeof(sockaddr_in)) < 0 )
  {
  	cout << "ERROR binding" << endl;
  	return -1;
  }

  if ( listen(serverSock, 10) < 0 )
  {
  	cout << "ERROR listening" << endl;
  	return -1;
  }

  // After listening, we have to send REQUEST message to enter CS here with thread
  // try to connect other nodes
  pthread_create( &csThread, 0, ProcessCriticalSection, NULL );
  pthread_detach( csThread );


  while ( 1 )
  {
  	conn = (Connection*)malloc(sizeof(Connection));

  	if ( conn == NULL )
  	{
  		printf("ERROR Memory allocation\n");
  		return -1;
  	}

  	conn->sockDesc = accept( serverSock, (struct sockaddr *) &conn->clientAddr, (socklen_t *)&conn->addrLen );

  	//printf("sockDesc: %d\n", conn->sockDesc);

  	if ( conn->sockDesc <= 0 )
  	{
  		free( conn );
  		cout << "ERROR accepting" << endl;
  		return -1;
  	}
  	else
  	{
  		pthread_create( &connThread, 0, ProcessControlMessage, (void *)conn );
  		pthread_detach( connThread );
  	}
  }

  printf("Computation completes\n");
  close( serverSock );
  free( conn );
  return 0;
}


