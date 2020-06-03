#include "stdafx.h"


int main(int argc, char *argv[])
{

    node_num = 8;
    thread_num = 2*node_num;
    msgNum = 0;

    pthread_t* threads =  (pthread_t*)malloc(sizeof(pthread_t)*thread_num);
	int* threadid = (int*)malloc(sizeof(int)*thread_num);
    // Create server node and make them into wait state
    for(int i = 0; i < node_num; i++){
		threadid[i*2] = i*2;
		pthread_create(&threads[i*2], NULL, serverNode, &threadid[i*2]);
	}

    usleep(2000*1000);
    for(int i = 0; i < node_num; i++){
        threadid[i*2 + 1] = i*2 + 1;
		pthread_create(&threads[i*2+1], NULL, clientNode, &threadid[i*2+1]);
	}
    for( int i = 0; i < thread_num; i++)
		pthread_join(threads[i], NULL);

    printf("Message: %d.\n", msgNum);
    free(threads); free(threadid);
    return EXIT_SUCCESS;
}


void* serverNode( void* arg)
{
    int thread_id, node_id;
	thread_id = *((int*)arg);
    node_id = thread_id/2;

    // Initialize for port
    int port = PORT_RANGE_START + node_id;
    portNums[node_id] = port;

    // Init for receiver
    int socket_fd = serverInit(port);
    if( socket_fd < 0)
    {
        printf("Error: Failed to create server %d.\n", node_id);
        return 0;
    }
    else{
        printf("Sucessful create server %d.\n", node_id);
        sockReceive[node_id] = socket_fd;
    }

    // Assume only one client connected to the server
    struct sockaddr_in addr_;
    socklen_t len_ = sizeof(addr_);
    int client = accept(socket_fd, (struct sockaddr*)&addr_, &len_);;

    // Initialize Lamport vector timestamp
    int timeStampNow[MAX_NODES]  = {0};

    // Initialize message packet
    MessagePacket sendPkt, receivePkt;
    char buffer[MAX_BUFFER_SIZE];
    int nBytesRead;

    // Initialize for request queue, and size of queue
    std::list<QueueData> queue;
    int len_queue;

    // Initialize for ackState
    bool ackState[MAX_NODES] = {false};
    bool requestLock = false;

    int loc, ackNum;

    while(1)
    {
        nBytesRead = recv(client, buffer, sizeof(MessagePacket), 0);
        // printf("size of buffer: %d. received %d: nodeid %d\n", sizeof(MessagePacket), nBytesRead, node_id);
        
        if(nBytesRead != 0)
		{
            __sync_add_and_fetch(&msgNum, 1);
            receivePkt = messageToPkt(buffer);
            // Update timeStampNow
            for(int i = 0; i < node_num; i++)
            {
                timeStampNow[i] = std::max(timeStampNow[i], receivePkt.timeStamp[i]);
            }
            timeStampNow[node_id] += 1;

            // Messgae driven processing
            switch(receivePkt.msgType)
            {
            case CS_INITMUTEXREQUEST:
                printf("Receive CS_INITMUTEXREQUEST in node %d.\n", node_id);
                // Send lock request to all other node
                len_queue = queue.size();
                timeStampNow[node_id]++;
                for(int i = 0; i < node_num; i++)
                {
                    if(i == node_id)
                    {
                        // inseret into current queue
                        // define a QueueData
                        QueueData enQueue;
                        enQueue.nodeID = node_id;
                        memcpy(enQueue.timeStamp, timeStampNow, sizeof(int)*MAX_NODES);
                        loc = insertIntoQueue(queue, enQueue);
                    }
                    else
                    {
                        memset(&sendPkt, 0, sizeof(MessagePacket));
                        sendPkt.msgType = CS_REQUEST;
                        sendPkt.senderId = node_id;
                        sendPkt.receiverId = i;
                        memcpy(sendPkt.timeStamp, timeStampNow, sizeof(int)*MAX_NODES);
                        sendPktToNode(sendPkt, i);
                    }                    
                }
                break;
            case CS_REQUEST:
                printf("Receive CS_REQUEST in node %d.\n", node_id);
                // Add into queue
                QueueData enQueue;
                enQueue.nodeID = receivePkt.senderId;
                memcpy(enQueue.timeStamp, receivePkt.timeStamp, sizeof(int)*MAX_NODES);
                loc = insertIntoQueue(queue, enQueue);
                // Check if send ack
                if(loc == 0)
                {
                    // Send Ack
                    timeStampNow[node_id]++;
                    memset(&sendPkt, 0, sizeof(MessagePacket));
                    sendPkt.msgType = CS_ACK;
                    sendPkt.senderId = node_id;
                    sendPkt.receiverId = receivePkt.senderId;
                    memcpy(sendPkt.timeStamp, timeStampNow, sizeof(int)*MAX_NODES);
                    sendPktToNode(sendPkt, receivePkt.senderId);
                }
                break;
            case CS_ACK:
                printf("Receive CS_ACK in node %d.\n", node_id);
                // Check if send complete
                ackState[receivePkt.senderId] = true;
                ackNum = 0;
                for(int k = 0; k < node_num; k++)
                {
                    if(ackState[k] == true)
                    {
                        ackNum++;
                    }
                }
                // If get all ack
                if( ackNum == node_num - 1)
                {
                    // Get the lock
                    usleep(1000);
                    printf("Successful get lock: %d\n", node_id);
                    // Release the lock
                    // Send complete message
                    timeStampNow[node_id]++;
                    memset(&sendPkt, 0, sizeof(MessagePacket));
                    sendPkt.msgType = CS_COMPLETION;
                    sendPkt.senderId = node_id;
                    memcpy(sendPkt.timeStamp, timeStampNow, sizeof(int)*MAX_NODES);
                    for(int i = 0; i < node_num; i++)
                    {
                        if ( i == node_id)
                        {
                            // Out of the queue
                            queue.pop_front();
                            for(int k = 0; k < node_num; k++)
                            {
                                ackState[k] = false;
                            }
                        }
                        else{
                            sendPkt.receiverId = i;
                            sendPktToNode(sendPkt, sendPkt.receiverId);
                        }
                    } 
                }
                break;
            case CS_COMPLETION:
                printf("Receive CS_COMPLETION in node %d.\n", node_id);
                // Remove the first
                queue.pop_front();
                // Send ack to the next
                len_queue = queue.size();
                if(len_queue > 0)
                {
                    timeStampNow[node_id]++;
                    memset(&sendPkt, 0, sizeof(MessagePacket));
                    sendPkt.msgType = CS_ACK;
                    sendPkt.senderId = node_id;
                    sendPkt.receiverId = queue.front().nodeID;
                    memcpy(sendPkt.timeStamp, timeStampNow, sizeof(int)*MAX_NODES);
                    sendPktToNode(sendPkt, sendPkt.receiverId);
                }
                break;
            case CS_EXIT:
                pthread_exit(0);
                break;
            defalut:
                break;
            }
        }
        // usleep(100);
    }
    return (void*)0;
}



void* clientNode( void* arg)
{
    int thread_id, node_id;
	thread_id = *((int*)arg);
    node_id = thread_id/2;
    
    int port = PORT_RANGE_START + node_id;
    // Init for port
    portNums[thread_id] = port;

    printf("Start connect to server %d.\n", node_id);
    char server_inaddr[1024] = {"127.0.0.1"};
    sockSend[node_id] = clientInit( server_inaddr, port);
    int socket_fd = sockSend[node_id];
    
    usleep(10*1000);
    if( socket_fd < 0)
    {
        printf("Error: Failed to create client %d.\n", node_id);
        return 0;
    }

    
    int nBytesSend;
    MessagePacket* sendPkt;
    while(1)
    {
        sendPkt = (MessagePacket*)malloc(sizeof(MessagePacket));
		memset(sendPkt, 0, sizeof(MessagePacket));
        sendPkt->msgType = CS_INITMUTEXREQUEST;
        sendPkt->senderId = node_id;
        sendPkt->receiverId = node_id;
        nBytesSend = send(socket_fd, sendPkt, sizeof(MessagePacket), 0);
        if(nBytesSend > 0)
        {
            printf("T -- write to node %d success [%d]\n", node_id, sendPkt->msgType);
        }
        else
        {
            perror("Error?");
            printf("Failed to send. %d\n", nBytesSend);
        }

        usleep(2000*1000);
        sendPkt->msgType = CS_EXIT;
        sendPktToNode(*sendPkt, node_id);
        free(sendPkt);
        pthread_exit(0);
    }
    return (void*)0;

}


MessagePacket messageToPkt(char* msg)
{
    MessagePacket retPkt;
    memcpy(&retPkt, msg, sizeof(MessagePacket));
    return retPkt;
}


int timeStampLess(int* timeStamp1, int* timeStamp2)
{
    // assume with node_num length
    int i, ret0, ret1, ret2;
    for(i = 0, ret0 = 0, ret1 = 0, ret2 = 0; i < node_num; i++)
    {
        if(timeStamp1[i] > timeStamp2[i])
        {
            ret1 += 1;
        }else if(timeStamp1[i] == timeStamp2[i])
        {
            ret0 += 1;
        }else{
            ret2 += 1;
        }
    }
    int ret;

    if(ret1 == 0 && ret2 > 0)
    {
        ret = -1;
    } else if(ret2 == 0 && ret1 > 0)
    {
        ret = 1;
    }else if(ret1 == 0 && ret2 == 0)
    {
        ret = 0;
    }else{
        ret = 2; // not sure which first
    }

    // return ret = -1, timeStamp1 < timeStamp2
    // return ret = 1, timeStamp1 > timeStamp2
    return ret;
}


int serverInit(int port)
{
    struct sockaddr_in server;
    int socket_desc;

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket in serverInit.\n");
        return -1;
    }

    //Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	
	// Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("Error: Bind failed in serverInit.");
		return -2;
	}

    // Listen
    if( -1 == listen(socket_desc, 5))
    {
        perror("Error: Create listen socket failed in serverInit.");
        return -3;
    }
    return socket_desc;
}


int clientInit(char *serv_addr, int port)
{
    struct sockaddr_in server;
    int socket_desc;
    
    //Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Error: Could not create socket in clientInit.\n");
        return -1;
	}
	
	server.sin_addr.s_addr = inet_addr(serv_addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	//Connect to remote server
	if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("Error: Connect failed in clientInit.\n");
		return -2;
	}
	return socket_desc;
}


int sendPktToNode(MessagePacket msgPkt, int receiverId)
{
    int nBytesSend, ret = 0;
    pthread_mutex_lock(&dataMutex);
    nBytesSend = send(sockSend[receiverId], &msgPkt, sizeof(msgPkt), 0);
    if(nBytesSend > 0)
    {
        // printf("Send msgPkt to node %d.\n", receiverId);
    }
    else
    {
        perror("Error: Failed to send.");
        ret = -1;
    }
    pthread_mutex_unlock(&dataMutex);
    return ret;
}

int insertIntoQueue(std::list<QueueData> &queue, QueueData &enQueue)
{
    int insertFlag = -1;
    int sizeQueue = queue.size();
    if (sizeQueue == 0)
    {
        queue.push_front(enQueue);
        insertFlag = 0;
    }
    else
    {
        for(std::list<QueueData>::iterator it = queue.begin(); it != queue.end(); it++)
        {
            int ret = timeStampLess(enQueue.timeStamp, it->timeStamp); 
            if( ret == -1)
            {
                insertFlag = (it == queue.begin())? 0:1;
                queue.insert(it, enQueue);
                break;
            } else if( ret == 0 || ret == 2)
            {
                if(enQueue.nodeID < it->nodeID)
                {
                    insertFlag = (it == queue.begin())? 0:1;
                    queue.insert(it, enQueue);
                    break;
                }
            }
        }
        if(insertFlag == -1)
        {
            insertFlag = queue.size();
            queue.push_back(enQueue);
        }
    }
    return insertFlag;
}