#ifndef STDAFX_H
#define STDAFX_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <list>


#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#define MAX_NODES           64
#define PORT_RANGE_START    1126
#define MAX_BUFFER_SIZE     1024

/***************************************
 * GLOBAL VARIABLE Declaration
 * *************************************/

static int sockSend[MAX_NODES];
static int sockReceive[MAX_NODES];
static int portNums[MAX_NODES];
static int node_num, thread_num;

static int msgNum;

static pthread_t enThread;
static pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;


/***************************************
 * ENUM Declaration
 * *************************************/
typedef enum
{
    CS_INITMUTEXREQUEST,
	CS_REQUEST,
	CS_ACK,
	CS_COMPLETION,
    CS_EXIT
}MessageType;


typedef struct
{
	MessageType     msgType;
    int             senderId;
    int             receiverId;
    int             timeStamp[MAX_NODES];
}MessagePacket;



typedef struct
{
    int nodeID;                 // Request node
    int timeStamp[MAX_NODES];   // Request timestamp
}QueueData;


/***************************************
 * FUNCTION Declaration
 * *************************************/
void* serverNode( void* arg);
void* clientNode( void* arg);
int serverInit(int port);
int clientInit(char *serv_addr, int port);
MessagePacket messageToPkt(char* msg);
int sendPktToNode(MessagePacket msgPkt, int receriverId);
int insertIntoQueue(std::list<QueueData> &queue, QueueData &enQueue);

#endif