#include "node.h"

using namespace std;

// Static variables
int Node::myid;
int Node::seq_no;
int Node::num_message_send;
int Node::num_message_recv;
int Node::highest_seq_num;
int Node::request_count, Node::reply_count;
bool Node::using_CS, Node::waiting_CS;
bool Node::all_nodes_connected;
bool Node::received_all_reply;
bool Node::exit_session;

int Node::sockfd[MAX_NUM_NODES];
bool Node::active_connection[MAX_NUM_NODES];
bool Node::reply_from_node[MAX_NUM_NODES];
bool Node::defer_node[MAX_NUM_NODES];
bool Node::complete_node[MAX_NUM_NODES];
struct sockaddr_in Node::serv_addr[MAX_NUM_NODES];

// Function declared
int generateRandomeNumber(int min, int max);
string messageSerialization(Message m);
Message messageDeserialization(char *s);


string messageSerialization(Message m)
{
	// type,my_id,seq_no
	string res = "";
	res += ( m.type + "," + to_string( m.my_id ) + "," + to_string( m.seq_no ) );
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
	m.seq_no = atoi( ss.substr( begin, pos ).c_str() );

	//printf("type: %s, id: %d, seq_no: %d\n", m.type.c_str(), m.my_id, m.seq_no);

	return m;
}

// Generate random number between a and b
int generateRandomeNumber( int min, int max )
{
	return rand() % min + max;
}

Node::Node( int id )
{
	MyThread::InitMutex( );
	Node::myid = id;
	this->nodeInit( );
	this->server_sock = socket( AF_INET, SOCK_STREAM, 0 );

	if ( this->server_sock <= 0 )
	{
		cout << "ERROR creating server socket" << endl;
		return;
	}

	memset( &this->server_addr, 0, sizeof( sockaddr_in ) );
	this->server_addr.sin_family = AF_INET;
	this->server_addr.sin_addr.s_addr = INADDR_ANY;
	this->server_addr.sin_port = htons( portno[myid] );

	int yes = 1;
	setsockopt( this->server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) );

	if ( bind(this->server_sock, (struct sockaddr *) &this->server_addr, sizeof(sockaddr_in)) < 0 )
  	{
  		cout << "ERROR binding" << endl;
  		return;
  	}

  	if ( listen( this->server_sock, 10 ) < 0 )
  	{
  		cout << "ERROR listening" << endl;
  		return;
  	}
}

void Node::nodeInit()
{
	hostent *host;

	Node::seq_no               = 0;
	Node::num_message_send     = 0;
	Node::num_message_recv     = 0;
	Node::highest_seq_num      = 0;
	Node::using_CS             = false;
	Node::waiting_CS           = false;
	Node::all_nodes_connected  = false;
	Node::received_all_reply   = false;
	Node::exit_session         = false;

	for ( int i = 0; i < MAX_NUM_NODES; i++ )
	{
		Node::active_connection[i] = false;
		Node::reply_from_node[i]   = false;
		Node::defer_node[i]        = false;
		Node::complete_node[i]     = false;

		Node::portno[i] = PORT_START + i;

		if ( i != Node::myid )
		{
			Node::sockfd[i] = socket( AF_INET, SOCK_STREAM, 0 );

			if ( Node::sockfd[i] <= 0 )
			{
				cerr << "ERROR opening socket" << endl;
			}

			memset( &Node::serv_addr[i], 0, sizeof(sockaddr_in) );
			Node::serv_addr[i].sin_family = AF_INET;
			Node::serv_addr[i].sin_port   = htons( this->portno[i] );

			string num_str = (( (i + 1) < 10 )? "0" : "") + to_string( i + 1 );
			string hostname = "dc" + num_str + ".utdallas.edu";
			//host = gethostbyname( hostname.c_str() );
			host = gethostbyname( "127.0.0.1" );

			if ( host == NULL )
			{
				cout << "ERROR host" << endl;
			}
			else
			{
				memcpy(&Node::serv_addr[i].sin_addr, host->h_addr_list[0], host->h_length);
			}

			if ( connect(Node::sockfd[i], (struct sockaddr *) &Node::serv_addr[i], sizeof(Node::serv_addr[i])) == -1 )
			{
				cout << "node " << i << " doesn't launch yet..." << endl;
			}
			else
			{
				Node::active_connection[i] = true;
			}
		}
	}

	return;
}

void Node::SendRequestAndEnterCS( )
{
	MyThread *t;
	t = new MyThread();

	t->Create( (void*)Node::ProcessCriticalSection, NULL );
	t->Detach( );
}

void Node::AcceptAndDistpatch( )
{
	Connection *conn;
	MyThread *t;

	while ( 1 )
	{
		conn = (Connection *)malloc( sizeof( Connection ) );

		if ( conn == NULL )
		{
			cout << "ERROR Memory allocation" << endl;
  			return;
		}

		conn->sockDesc = accept( this->server_sock, (struct sockaddr *) &conn->clientAddr, (socklen_t *)&conn->addrLen );
		t = new MyThread();

		if ( conn->sockDesc <= 0 )
		{
			free( conn );
			cout << "ERROR accepting" << endl;
  			return;
		}
		else
		{
			t->Create( (void *)Node::ProcessControlMessage, (void *)conn );
			t->Detach( );
		}

		if ( Node::exit_session )
		{
			break;
		}
	}

	printf("Computation completes\n");
  	close( this->server_sock );
  	free( conn );

  	return;
}

void *Node::ProcessControlMessage( void *args )
{
	Connection* conn;
	int numBytesRead = 0;
	int numBytesSend = 0;
	int count_reply  = 0;
	int counter      = 0;
	bool myPriority  = false;
	bool exitCompute = false;

	if ( args == NULL )
	{
		pthread_exit(0);
	}

	conn = (Connection *)args;

	while ( 1 )
	{
		Message m;
		char buffer[256];

		numBytesRead = recv( conn->sockDesc, buffer, 256, 0 );
		m = messageDeserialization( buffer );

		if ( numBytesRead == 0 )
		{
			break;
		}

		if ( m.type == "REQUEST" )
		{
			MyThread::LockMutex( (const char *)m.type.c_str() );

			while ( 1 )
			{
				if ( all_nodes_connected )
				{
					break;
				}
			}

			/* Record the highest comming seq_no */
			if ( m.seq_no > Node::highest_seq_num )
			{
				Node::highest_seq_num = m.seq_no;
			}

			Node::num_message_recv++;

			if ( m.seq_no > Node::seq_no || ( m.seq_no == Node::seq_no && m.my_id > Node::myid ) )
			{
				myPriority = true;
			}

			if ( Node::using_CS || ( Node::waiting_CS && myPriority ) )
			{
				/*
				If this node is using CS or waiting CS and got priority,
				then defering the comming REQUEST.
				 */
				Node::defer_node[m.my_id] = true;
				//printf( "ProcessControlMessage() -- Case 1: Defer comming REQUEST.\n" );

			}
			else if ( ( !Node::using_CS || !Node::waiting_CS ) || ( Node::waiting_CS && !Node::reply_from_node[m.my_id] && !myPriority ) )
			{
				/*
				If this node is not using CS or not waiting CS, or waiting CS but not receiving REPLY from m.my_id
				and not its priority, then, send REPLY to m.my_id.
				 */

				Node::reply_from_node[m.my_id] = false;
				Node::received_all_reply = false;

				Message rpy("REPLY", myid, 0);
				string msg = messageSerialization( rpy );

				Node::num_message_send++;
				send( Node::sockfd[m.my_id], msg.c_str(), strlen( msg.c_str() ), 0 );
				//printf( "ProcessControlMessage() -- Case2: Send REPLY message to node.\n" );

			}
			else if ( Node::waiting_CS && Node::reply_from_node[m.my_id] && !myPriority )
			{
				/*
				If this node is waiting CS and got REPLY from m.my_id but not its priority,
				then, send REPLY to m.ny_id.
				 */

				Node::reply_from_node[m.my_id] = false;
				Node::received_all_reply = false;

				Message rpy("REPLY", myid, 0);
				string msg = messageSerialization( rpy );

				Node::num_message_send++;
				Node::request_count++;
				send( Node::sockfd[m.my_id], msg.c_str(), strlen( msg.c_str() ), 0 );
				//printf( "ProcessControlMessage() -- Case3: Send REPLY message to node.\n" );

				usleep( 10000 );

				Message r( "REQUEST", myid, seq_no );
				string rr = messageSerialization( r );

				Node::num_message_send++;
				send( Node::sockfd[m.my_id], rr.c_str(), strlen( rr.c_str() ), 0 );
			}

			MyThread::UnlockMutex( (const char *)m.type.c_str() );
		}
		else if ( m.type == "REPLY" )
		{
			counter = 0;

			MyThread::LockMutex( (const char *)m.type.c_str() );

			Node::reply_count++;
			Node::num_message_recv++;
			count_reply++;
			Node::reply_from_node[m.my_id] = true;

			if ( !Node::received_all_reply )
			{
				for ( int i = 0; i < MAX_NUM_NODES; i++ )
				{
					if ( i != Node::myid && Node::reply_from_node[i] )
					{
						counter++;
					}
				}

				if ( counter == MAX_NUM_NODES - 1 )
				{
					Node::received_all_reply = true;
				}
			}

			MyThread::UnlockMutex( (const char *)m.type.c_str() );
		}
		else if ( m.type == "COMPLETE" )
		{
			MyThread::LockMutex( (const char *)m.type.c_str() );

			Node::num_message_recv++;

			if ( Node::myid == 0 )
			{
				exitCompute = true;
				Node::complete_node[m.my_id] = true;

				for ( int i = 0; i < MAX_NUM_NODES; i++ )
				{
					if ( !Node::complete_node[i] )
					{
						exitCompute = false;
						break;
					}
				}
				if ( exitCompute )
				{
					//printf("Exit compute is true!\n");

					Node::exit_session = true;

					Message c( "COMPLETE", myid, 0 );
					string cc = messageSerialization( c );

					for ( int i = 0; i < MAX_NUM_NODES; i++ )
					{
						if ( i != Node::myid )
						{
							Node::num_message_send++;
							send( Node::sockfd[i], cc.c_str(), strlen(cc.c_str()), 0 );
						}
					}
				}
			}
			else if (m.my_id == 0)
			{
				Node::exit_session = true;
			}

			MyThread::UnlockMutex( (const char *)m.type.c_str() );
		}

		MyThread::LockMutex( "exitSession" );

		if ( exit_session )
		{
			for ( int i = 0; i < MAX_NUM_NODES; i++ )
			{
				if ( i != myid )
				{
					close( sockfd[i] );
				}
			}
			MyThread::UnlockMutex( "exitSession" );
			break;
		}

		MyThread::UnlockMutex( "exitSession" );
	}

	printf("Exit Session ProcessControl() thread\n");

	Connection *con = (Connection *)args;
	close( con->sockDesc );
	free( con );

	pthread_exit( 0 );
}

void *Node::ProcessCriticalSection( void *args )
{
	int time_to_wait = 0;
	int no_cs_entry = 0;
	struct timeval tv;
	long begin, end;

	MyThread::LockMutex( "Connect" );

	for ( int i = 0; i < MAX_NUM_NODES; i++ )
	{
		if ( i == Node::myid )
		{
			continue;
		}

		while ( Node::active_connection[i] == false )
		{
			Node::sockfd[i] = socket( AF_INET, SOCK_STREAM, 0 );

			if ( connect( Node::sockfd[i], (struct sockaddr *) &Node::serv_addr[i], sizeof( Node::serv_addr[i]) ) > -1 )
			{
				cout << "Successfully connecting to node: " << i << endl;
				Node::active_connection[i] = true;
			}
			else
			{
				close( Node::sockfd[i] );
			}
		}
	}
	Node::all_nodes_connected = true;

	MyThread::UnlockMutex( "Connect" );

	while ( no_cs_entry < MAX_CS_ENTRY )
	{
		MyThread::LockMutex( "Wait" );

		if ( Node::myid % 2 == 0 )
		{
			MyThread::UnlockMutex( "Wait" );
			time_to_wait = generateRandomeNumber( 5, 10 );
			usleep( 10000 * time_to_wait );
		}
		else
		{
			MyThread::UnlockMutex( "Wait" );
			if ( no_cs_entry > 20 )
			{
				time_to_wait = generateRandomeNumber( 45, 50 );
				usleep( 10000 * time_to_wait );
			}
			else
			{
				time_to_wait = generateRandomeNumber( 5, 10 );
				usleep( 10000 * time_to_wait );
			}
		}

		MyThread::LockMutex( "SendRequest" );

		Node::request_count = 0;
		Node::reply_count = 0;
		Node::waiting_CS = true;
		Node::seq_no = Node::highest_seq_num + 1;

		Message m("REQUEST", myid, seq_no);
		string mm = messageSerialization( m );

		gettimeofday( &tv, NULL );
		begin = tv.tv_sec * 1000 + tv.tv_usec / 1000;

		for ( int i = 0; i < MAX_NUM_NODES; i++ )
		{
			/* Send REQUEST only when there is no REPLY. */
			if ( i != Node::myid && !Node::reply_from_node[i] )
			{
				Node::request_count++;
				Node::num_message_send++;
				send( Node::sockfd[i], mm.c_str(), strlen( mm.c_str() ), 0 );
			}
		}

		MyThread::UnlockMutex( "SendRequest" );

		while ( 1 )
		{
			MyThread::LockMutex( "EnterCS" );

			if ( Node::received_all_reply )
			{
				gettimeofday( &tv, NULL );
				end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
				Node::using_CS = true;
				Node::waiting_CS = false;
				no_cs_entry++;

				printf("REQUESTs: %d, REPLYs: %d\n", Node::request_count, Node::reply_count);
				printf("Time elapsed: %ld \n", end - begin);
				printf("Entering...\n");
				usleep( 30000 );
				MyThread::UnlockMutex( "EnterCS" );
				break;
			}
			else
			{
				MyThread::UnlockMutex( "EnterCS" );
			}
		}

		MyThread::LockMutex( "ReplyDefer" );

		Node::using_CS = false;

		Message r("REPLY", myid, 0);
		string rr = messageSerialization( r );

		for ( int i = 0; i < MAX_NUM_NODES; i++ )
		{
			/* reply to all nodes that was defered. */
			if ( i != Node::myid && Node::defer_node[i] )
			{
				Node::num_message_send++;
				Node::defer_node[i] 	   = false;
				Node::reply_from_node[i] = false;
				Node::received_all_reply   = false;

				send( Node::sockfd[i], rr.c_str(), strlen( rr.c_str() ), 0 );
			}
		}

		MyThread::UnlockMutex( "ReplyDefer" );
	}

	MyThread::LockMutex( "Complete" );

	if ( Node::myid != 0 )
	{
		Message complete( "COMPLETE", myid, 0 );
		string cc = messageSerialization( complete );

		Node::num_message_send++;
		send( Node::sockfd[0], cc.c_str(), strlen( cc.c_str() ) + 1, 0 );
	}
	else
	{
		Node::complete_node[0] = true;
	}

	MyThread::UnlockMutex( "Complete" );

	printf( "Node ID: %d\n", Node::myid );
	printf( "Total sending message: %d\n", Node::num_message_send );
	printf( "Total receiving message: %d\n", Node::num_message_recv );
	printf( "Exit Session ProcessCriticalSection() thread\n" );
	pthread_exit( 0 );
}

