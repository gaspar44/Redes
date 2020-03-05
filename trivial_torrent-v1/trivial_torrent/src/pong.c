/**
 * This file implements a "UDP ping server".
 *
 * It basically waits for datagrams to arrive, and for each one received, it responds to the original sender
 * with another datagram that has the same payload as the original datagram. The server must reply to 3
 * datagrams and then quit.
 *
 * Test with:
 *
 * $ netcat localhost 8080
 * ping
 * ping
 * pong
 * pong
 *
 * (assuming that this program listens at localhost port 8080)
 *
 */

// TODO: some includes here
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#define PORT 8080
#define BUFSIZE 2048

int main(int argc, char **argv) {

	(void) argc; // This is how an unused parameter warning is silenced.
	(void) argv;

	struct sockaddr_in clientAddress;
	struct sockaddr_in serverAddress;
	socklen_t addressLen = sizeof(clientAddress);
	ssize_t recvLen;
	int socketFileDescriptor;
	unsigned char messageReaded[BUFSIZE];

	if ( (socketFileDescriptor = socket(AF_INET,SOCK_DGRAM,0)) < 0){
		perror("error opening socket");
		return(EXIT_FAILURE);
	}

	// Bind part

	memset((char *)&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT);

	if ( bind(socketFileDescriptor, (struct sockaddr*)&serverAddress,sizeof(serverAddress)) < 0){
		perror("Error binding port");
		return(EXIT_FAILURE);
	}

	int messagesToRecibe = 3;

	while (messagesToRecibe != 0){
		recvLen = recvfrom(socketFileDescriptor,messageReaded,BUFSIZE,0,(struct sockaddr *)& clientAddress,&addressLen);
		
		if (recvLen > 0){
			unsigned int bytesToSendBack = (unsigned int ) recvLen;

			ssize_t error = sendto(socketFileDescriptor,messageReaded,bytesToSendBack,0,(struct sockaddr*)&clientAddress,sizeof(clientAddress));

			if (error > 0) {
				messagesToRecibe--;
			}
		}
	}
	
	shutdown(socketFileDescriptor, 2);  // Force the unbind. Should work better with TCP instead of UDP
	close(socketFileDescriptor);
	return 0;
}
