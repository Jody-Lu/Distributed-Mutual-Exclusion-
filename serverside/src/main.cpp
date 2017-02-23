#include "node.h"

using namespace std;

int main(int argc, char const *argv[])
{
	Node *node = new Node( atoi(argv[1]) );
	node->SendRequestAndEnterCS( );
	node->AcceptAndDistpatch( );

	return 0;
}