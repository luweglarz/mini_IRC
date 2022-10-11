#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client{
    int id;
    int fd;
    char msg[42 * 4096];
    struct s_client *next;
}               t_client;


t_client *clients;

void exitFatal(int serverSocket){
    close(serverSocket);
    write(2, "Fatal error\n", 12);
    exit(1);
}

int getMaxFds(int serverSocket){
    int max = serverSocket;
    t_client *tmpClients = clients;

    while(tmpClients){
        if(max < tmpClients->fd)
            max = tmpClients->fd;
        tmpClients = tmpClients->next;
    }
    return max;
}

int serverSetup(char *port){
    int serverSocket;
    struct sockaddr_in servaddr;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0); 
	if (serverSocket == -1)
        exitFatal(serverSocket);
	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(port)); 
   
	if ((bind(serverSocket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
        exitFatal(serverSocket);
	if (listen(serverSocket, 10) == -1)
        exitFatal(serverSocket);
    return serverSocket;
}

void sendToClients(int serverSocket, int sender, fd_set *writeSet, char *msg){
    t_client *tmpClients = clients;

    while(tmpClients){
        printf("loop\n");
        if(FD_ISSET(tmpClients->fd, writeSet) && sender != tmpClients->fd)
            if(send(tmpClients->fd, msg, strlen(msg), 0) == -1)
                exitFatal(serverSocket);
        tmpClients = tmpClients->next;
    }
}

void addClient(int clientSocket, int serverSocket, fd_set *writeSet){
    char msg[1024];
    t_client *tmpClients = clients;
    t_client *newClient = NULL;
    static int id = 0;

    if(!(newClient = calloc(1, sizeof(t_client))))
        exitFatal(serverSocket);

    newClient->fd = clientSocket;
    newClient->id = ++id;
    bzero(&newClient->msg, sizeof(newClient->msg));
    newClient->next = NULL;
    if(clients == NULL){
        clients = newClient;
    
    }
    else{
        while( tmpClients->next)
            tmpClients = tmpClients->next;
        tmpClients->next = newClient;
    }
    sprintf(msg, "server: client %d arrived\n", newClient->id);
    sendToClients(serverSocket, newClient->fd, writeSet, msg);
}

void acceptClient(int serverSocket, fd_set *serverSet, fd_set *writeSet){
    int clientSocket;
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

	clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &len);
	if (accept < 0)
        exitFatal(serverSocket);
    addClient(clientSocket, serverSocket, writeSet);
    FD_SET(clientSocket, serverSet);
}

void deleteClient(int clientSocket, int serverSocket, fd_set *writeSet, fd_set *serverSet){
    t_client *tmpClients = clients;
    t_client *toDell;
    char msg[1024];

    if(clients == NULL)
        return;
    if (clients->fd == clientSocket){
        toDell = clients;
        clients = NULL;
        close(toDell->fd);
        FD_CLR(toDell->fd, serverSet);
        free(toDell);
    }
    else{
        while(tmpClients){
            if (tmpClients->next->fd == clientSocket){
                toDell = tmpClients->next;
                sprintf(msg, "server: client %d left\n", toDell->id);
                sendToClients(serverSocket, clientSocket, writeSet, msg);
                tmpClients->next = toDell->next;
                close(toDell->fd);
                FD_CLR(toDell->fd, serverSet);
                free(toDell);
                break;
            }
            tmpClients = tmpClients->next;
        }
    }
}

void checkMessage(int serverSocket, t_client *client, fd_set *writeSet, char *recvBuff){
    int i = 0;
    char sendBuff[42 * 4096 + 42];

    while (recvBuff[i]){
        client->msg[strlen(client->msg)] = recvBuff[i];
        if(recvBuff[i] == '\n'){
            sprintf(sendBuff, "client %d: %s", client->id, client->msg);
            sendToClients(serverSocket, client->fd, writeSet, sendBuff);
            bzero(&client->msg, sizeof(client->msg));
            bzero(&sendBuff, sizeof(sendBuff));
        }
        i++;
    }

}

int main(int ac, char **av) {
    t_client *tmpClients;
    char sendBuff[42 * 4096];
    char recvBuff[42 * 4096];
	int serverSocket;
    int max;
    int recvSize;
    fd_set serverSet, writeSet, readSet;
    
    if (ac !=2){
        write(2, "Wrong number of argument\n", 25);
        exit(1);
    }

    serverSocket = serverSetup(av[1]);

    bzero(&sendBuff, sizeof(sendBuff));
    bzero(&recvBuff, sizeof(recvBuff));
    FD_ZERO(&serverSet);
    FD_SET(serverSocket, &serverSet);
    while(1) {
        writeSet = serverSet;
        readSet = writeSet;
        max = getMaxFds(serverSocket);

        if (select(max + 1, &readSet, &writeSet, NULL, NULL) < 0)
            continue;
        if (FD_ISSET(serverSocket, &readSet))
            acceptClient(serverSocket, &serverSet, &writeSet);
        tmpClients = clients;
        while (tmpClients){
            if (FD_ISSET(tmpClients->fd, &readSet)){
                 recvSize = recv(tmpClients->fd, recvBuff + strlen(recvBuff), 42 * 4096, 0);
                 if (recvSize <= 0){
                     deleteClient(tmpClients->fd, serverSocket, &writeSet, &serverSet);
                     bzero(&recvBuff, sizeof(recvBuff));
                     break;
                 }
                 else{
                    checkMessage(serverSocket, tmpClients,&writeSet, recvBuff);
                    bzero(&recvBuff, sizeof(recvBuff));
                 }

            }

            tmpClients = tmpClients->next;
        }
    }
}
