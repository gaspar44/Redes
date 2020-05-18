#ifndef TTORRENT_H_
#define TTORRENT_H_

#include "file_io.h"
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// TODO: hey!? what is this?

// https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols

int checkValidPort(char *portCandidate);
int allocateServerStructs(int sockFd,struct sockaddr_in servaddr);
int createServer(int portToUse,struct torrent_t* metafileInfo);
int forkProcess(int newFileDescriptorToUse,struct torrent_t *metaFileInfo);
int recvMessageRequestFromClient(int socketFileDescriptor,char * serverRequestMessage);
uint64_t checkHeaderReceivedFromClient(char *clientRequest);
int sendResponseToClient(int socketFileDescriptor, char *blockToSend,uint64_t bytesToSendBack);
void waitForChildProccesses(int signal);

struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer,int isServer);
int startClient(struct torrent_t *metaInfoFile);
int checkHeaderReceivedFromServer(char * serverResponse,uint64_t blockNumber);
int downloadFile(struct torrent_t *metaFileInfo);
int createClientToServerConnection(struct torrent_t *metaFileInfo,uint64_t peerToRequest);

static const uint32_t MAGIC_NUMBER = 0xde1c3230;

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;
static const uint64_t ERROR_BLOCK = 9999;
static const int MAX_TIME_OUT = 10;

enum { RAW_MESSAGE_SIZE = 13,
	RAW_RESPONSE_SIZE = 13
};

int checkValidPort(char *portCandidate) {
	int port = atoi(portCandidate);

	if (port < 0) {
		log_printf(LOG_INFO,"error: the %s is not a valid port\n",portCandidate);
		return -1;
	}

	else if (port >= 0 && port <= 1024) {
		log_printf(LOG_INFO, "error: the port %d is a reserved port\n",port);
		return -1;
	}

	log_printf(LOG_INFO,"Server mode (port %d)\n",port);

	return port;
}

int allocateServerStructs(int sockfd,struct sockaddr_in servaddr) {
	if (fcntl(sockfd, F_SETFL, O_CLOEXEC) == -1){ // Necessary for stop address already in use after each test
			log_printf(LOG_INFO, "error fcntl 1\n");
			return -1;
		}

		if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1){
			log_printf(LOG_INFO, "error binding...\n");

			if (errno == 98){
				log_printf(LOG_INFO,"address already in use\n");
			}

			return -1;
		}

		log_printf(LOG_INFO, "Binding success!\n");

		if ( listen(sockfd, 10) == -1){
			log_printf(LOG_INFO, "error listening...\n");

			return -1;
		}

		return 0;
}

void waitForChildProccesses(int signal){
	(void) signal;
	log_printf(LOG_DEBUG, "Signal %d received\n",signal);
	pid_t pidToExit = -2;
	int status;
	log_printf(LOG_DEBUG, "pidToExit %d\n",pidToExit);

	while(pidToExit != 0 && pidToExit != -1){
		pidToExit = waitpid(-1,&status,WNOHANG); // See man wait for details
		log_printf(LOG_DEBUG, "wating for: %d\n",pidToExit);
	}
}

int forkProcess(int newFileDescriptorToUse,struct torrent_t *metaFileInfo){
	char *serverRequestMessage = (char*)malloc(RAW_MESSAGE_SIZE);

	while(1){
		memset(serverRequestMessage, 0, RAW_MESSAGE_SIZE);
		int recvError = recvMessageRequestFromClient(newFileDescriptorToUse, serverRequestMessage);

		if (recvError == -1){
			log_printf(LOG_DEBUG, "error from client: %d\n",errno);
			return EXIT_FAILURE;
		}

		uint64_t errorOrBlockToSend = checkHeaderReceivedFromClient(serverRequestMessage);

		if (errorOrBlockToSend == ERROR_BLOCK)
			return EXIT_FAILURE;

		if (errorOrBlockToSend >= metaFileInfo->block_count) {
			char *responseBlock = (char*)malloc(RAW_RESPONSE_SIZE);
			memcpy(responseBlock, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
			memcpy(responseBlock + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_NA,sizeof(MSG_RESPONSE_NA));
			memcpy(responseBlock + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &errorOrBlockToSend,sizeof(errorOrBlockToSend));

			int isSended = sendResponseToClient(newFileDescriptorToUse,responseBlock,RAW_RESPONSE_SIZE);

			if (isSended == -1){
				log_printf(LOG_INFO, "time out reached while sending back\n");
				return EXIT_FAILURE;
			}

			log_printf(LOG_INFO, "blocked not available\n");
			free(responseBlock);
			break;
		}

		else {
			log_printf(LOG_INFO, "sending response to client...\nMagic Number: 0x%X08\nfd: %d\nrequest status: %ld\nblock to send back: %ld\n",MAGIC_NUMBER,newFileDescriptorToUse,MSG_RESPONSE_OK,errorOrBlockToSend);
			struct block_t blockToSend;
			int isLoaded = load_block(metaFileInfo, errorOrBlockToSend, &blockToSend);

			if (isLoaded == -1){
				log_printf(LOG_INFO, "Error loading block\n");
				return EXIT_FAILURE;
			}

			char *responseBlock = (char*)malloc(RAW_RESPONSE_SIZE + blockToSend.size);
			memcpy(responseBlock,&MAGIC_NUMBER , sizeof(MAGIC_NUMBER));
			memcpy(responseBlock + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_OK,sizeof(MSG_RESPONSE_OK));
			memcpy(responseBlock + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &errorOrBlockToSend,sizeof(errorOrBlockToSend));
			memcpy(responseBlock + RAW_RESPONSE_SIZE, blockToSend.data, blockToSend.size);

			int isSended = sendResponseToClient(newFileDescriptorToUse,responseBlock,RAW_RESPONSE_SIZE + blockToSend.size);

			if (isSended == -1){
				log_printf(LOG_INFO, "time out reached while sending back\n");
				return EXIT_FAILURE;
			}

			free(responseBlock);
			if (errorOrBlockToSend == metaFileInfo->block_count - 1)
				break;
		}
	}

	free(serverRequestMessage);
	return EXIT_SUCCESS;
}

int createServer(int portToUse,struct torrent_t *metaFileInfo){

	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			log_printf(LOG_INFO, "error opening socket. Exiting...\n");
			return -1;
	}

	log_printf(LOG_INFO,"Socket created successfully\n");
	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((uint16_t)portToUse);

	int err = allocateServerStructs(sockfd,servaddr);

	if (err == -1)
		return -1;

	while(1){
		log_printf(LOG_INFO, "Listen to connections now\n");
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD,waitForChildProccesses);

		int newFileDescriptorToUse = accept(sockfd, NULL, NULL);
		if (newFileDescriptorToUse == -1){
			if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR){
				sleep(1);
				continue;
			}

			else {
				log_printf(LOG_INFO, "error while arriving connections, code: %d\n",errno);
				return -1;
			}
		}

		else {
			pid_t forkedID;

			if ( (forkedID = fork()) < 0){
				log_printf(LOG_INFO, "can not make a fork()\n");
				return -1;
			}

			else if (forkedID == 0){
				log_printf(LOG_INFO,"forked client ;). Closing unnecessary stuff\n");
				close(sockfd);
				int exitClientStatus = forkProcess(newFileDescriptorToUse,metaFileInfo);
				destroy_torrent(metaFileInfo);
				close(newFileDescriptorToUse);
				exit(exitClientStatus);
			}
		}

		close(newFileDescriptorToUse);
	}

	close(sockfd);
	return 0;
}

int recvMessageRequestFromClient(int socketFileDescriptor,char * serverRequestMessage) {
	ssize_t totalBytesReadedFromClient = 0;
	int requestTimeOut = 0;
	while (totalBytesReadedFromClient != RAW_MESSAGE_SIZE){
		ssize_t bytesReadedFromClient = recv(socketFileDescriptor,serverRequestMessage + totalBytesReadedFromClient,RAW_MESSAGE_SIZE,0);

		if (bytesReadedFromClient < 0){
			if (errno == EPIPE || errno == ECONNRESET)
				return -1;
			sleep(1);
		}

		else if (bytesReadedFromClient == 0){
			requestTimeOut = requestTimeOut + 1;
			sleep(1);
			if (requestTimeOut == MAX_TIME_OUT){
				return -1;
			}
		}

		else{
			log_printf(LOG_INFO,"readed Bytes: %ld\n",bytesReadedFromClient);
			totalBytesReadedFromClient = totalBytesReadedFromClient + bytesReadedFromClient;
		}
	}
	return 0;
}

uint64_t checkHeaderReceivedFromClient(char *clientRequest){
	log_printf(LOG_INFO,"checking header...");
	uint32_t *requestMagicNumber = (uint32_t *) clientRequest;
	uint8_t *requestMessage = (uint8_t *)(clientRequest + sizeof(MAGIC_NUMBER));
	uint64_t *requestBlockNumer = (uint64_t *) (clientRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK));
	log_printf(LOG_INFO, "Received from client:\nMagicNumber:0x%08X\nrequestMessage: %ld\nblock: %d\n",*requestMagicNumber,*requestMessage,*requestBlockNumer);

	if (*requestMessage != 0){
		log_printf(LOG_INFO, "invalid message from client\n");
		return ERROR_BLOCK;
	}

	if (*requestMagicNumber != MAGIC_NUMBER){
		log_printf(LOG_INFO, "invalid Magic number from client");
		return ERROR_BLOCK;
	}

	log_printf(LOG_INFO, "Valid header!");

	return *requestBlockNumer;
}

int sendResponseToClient(int socketFileDescriptor, char *blockToSend,uint64_t bytesToSendBack) {
	int timeOut = 0;
	ssize_t totalBytesSended = 0;

	while (timeOut != MAX_TIME_OUT && (uint64_t)totalBytesSended != bytesToSendBack){
		ssize_t bytesSendedBack = send(socketFileDescriptor,blockToSend,bytesToSendBack,0);

		if (bytesSendedBack < 0){
			if (errno == EPIPE || errno == ECONNRESET)
				return 0;

			log_printf(LOG_INFO, "error sending response: %d\n",errno);
			sleep(1);
			timeOut = timeOut + 1;
		}
		else {
			totalBytesSended = totalBytesSended + bytesSendedBack;
		}
	}

	return timeOut == MAX_TIME_OUT ? -1 : 0;
}

int checkHeaderReceivedFromServer(char * serverResponse,uint64_t blockNumber){
	log_printf(LOG_INFO,"checking header...");
	uint32_t *responseMagicNumber = (uint32_t *) serverResponse;
	uint8_t *responseMessage = (uint8_t *)(serverResponse + sizeof(MAGIC_NUMBER));
	uint64_t *responseBlockNumer = (uint64_t *) (serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK));

	if (*responseMessage == 0){
		log_printf(LOG_INFO, "invalid message from server\n");
		return -1;
	}

	if (*responseMessage == 2){
		return 2;
	}

	if (*responseMagicNumber != MAGIC_NUMBER){
		log_printf(LOG_INFO, "invalid Magic number from server: 0x%08X,expected: 0x%08X\n",*responseMagicNumber,MAGIC_NUMBER);
		return -1;
	}

	if (*responseBlockNumer != blockNumber){
		log_printf(LOG_INFO, "invalid block");
		return -1;
	}

	log_printf(LOG_INFO, "Valid header!");
	return 0;
}

int createClientToServerConnection(struct torrent_t *metaFileInfo,uint64_t peerToRequest) {
	if (peerToRequest >= metaFileInfo->peer_count){
		log_printf(LOG_INFO, "Invalid peer to request");
		return -1;
	}

	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("%d\n",errno);
		log_printf(LOG_INFO, "error opening socket. Exiting...\n");
		return -1;
	}

	log_printf(LOG_INFO,"Socket created successfully\n");
	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = *(uint32_t*)metaFileInfo->peers[peerToRequest].peer_address;
	servaddr.sin_port = metaFileInfo->peers[peerToRequest].peer_port;

	if ( (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) == -1) {
		log_printf(LOG_INFO, "error connecting socket. Exiting...\n");
		return -1;
	}

	log_printf(LOG_INFO,"Socket connected successfully\n");

	return sockfd;
}

int downloadFile(struct torrent_t *metaFileInfo){
	uint64_t peerToRequest = 0;
	for (uint64_t blockNumber = 0; blockNumber < metaFileInfo->block_count; blockNumber++){
		char *blockToRequest = (char*)malloc(RAW_MESSAGE_SIZE);
		int sockfd = createClientToServerConnection(metaFileInfo, peerToRequest);

		if (sockfd == -1 && errno == ECONNREFUSED && peerToRequest < metaFileInfo->peer_count){
			log_printf(LOG_INFO, "peer not avaliable, trying other\n");
			peerToRequest++;
			blockNumber--;
			continue;
		}

		else if (sockfd == -1 && errno != ECONNREFUSED)
			return sockfd;

		memcpy(blockToRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
		memcpy(blockToRequest + sizeof(MAGIC_NUMBER),&MSG_REQUEST,sizeof(MSG_REQUEST));
		memcpy(blockToRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));

		ssize_t error = send(sockfd,blockToRequest,RAW_MESSAGE_SIZE,0);

		if (error < 0){
			log_printf(LOG_INFO, "error received from server");
			exit(EXIT_FAILURE);
		}

		struct block_t block;
		block.size = get_block_size(metaFileInfo, blockNumber);

		char *serverResponse = malloc(RAW_RESPONSE_SIZE + block.size);
		ssize_t totalReadedBytes = 0;

		while (1) {
			ssize_t partialReadedBytes = recv(sockfd,serverResponse + totalReadedBytes,RAW_RESPONSE_SIZE + block.size,0);
			log_printf(LOG_INFO,"readed Bytes: %ld\n",partialReadedBytes);

			if (partialReadedBytes < 0) {
				log_printf(LOG_INFO, "error reading from server");
				exit(EXIT_FAILURE);
			}

			else {
				totalReadedBytes = totalReadedBytes + partialReadedBytes;
				if ((uint64_t)totalReadedBytes >= RAW_RESPONSE_SIZE + block.size){
					break;
				}
			}
		}
			int validHeader = checkHeaderReceivedFromServer(serverResponse,blockNumber);

			if (validHeader == -1){
				log_printf(LOG_INFO, "error in header");
				blockNumber--;

				free(blockToRequest);
				free(serverResponse);
				close(sockfd);
				continue;
			}

			if (validHeader == 2){
				free(blockToRequest);
				free(serverResponse);
				close(sockfd);
				return -1;
			}

			log_printf(LOG_INFO, "data of block %ld received\n",blockNumber);
			log_printf(LOG_INFO, "Verifying block...");
			uint8_t * blockData = (uint8_t *) (serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK) + sizeof(blockNumber));
			memcpy(block.data, blockData, block.size);
			int isValid = store_block(metaFileInfo, blockNumber, &block);

			if (isValid != 0){
				log_printf(LOG_INFO, "Invalid block, request again");
				blockNumber--;
			}

			else {
				log_printf(LOG_INFO, "Valid block");
			}

		free(blockToRequest);
		free(serverResponse);
		close(sockfd);
	}

	return 0;
}

struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer,int isServer){

	if ( strstr(metaFileToDownloadFromServer, ".ttorrent") == NULL) {
		log_printf(LOG_INFO,"ERROR: NO EXTENSION FOUND");
		exit(EXIT_FAILURE);
	}

	char *fileBeforeStrktok = calloc(sizeof(char), 2048);
	strcpy(fileBeforeStrktok,metaFileToDownloadFromServer);

	char * extension = strtok(metaFileToDownloadFromServer,".");
	char *extensionAfterChange = calloc(sizeof(char), 2048);

	while (extension != NULL){
		extension = strtok(NULL,".");
		strcpy(extensionAfterChange, extension);

		if (isServer && (strcmp(extensionAfterChange, "ttorrent") == 0)){
			FILE * fileToCheck;

			if ((fileToCheck = fopen(metaFileToDownloadFromServer, "r"))){
					fclose(fileToCheck);
			}

			else {
				log_printf(LOG_INFO, "file %s to use not found\n",extensionAfterChange);
				exit(EXIT_FAILURE);
			}

			free(extensionAfterChange);
			break;
		}

		else if (strcmp(extensionAfterChange, "ttorrent") == 0){
			break;
		}

		metaFileToDownloadFromServer = strcat(metaFileToDownloadFromServer,".");
		metaFileToDownloadFromServer = strcat(metaFileToDownloadFromServer,extensionAfterChange);
	}

	struct torrent_t metaFileInfo;

	int creationSuccess = create_torrent_from_metainfo_file(fileBeforeStrktok,&metaFileInfo,metaFileToDownloadFromServer);
	free(fileBeforeStrktok);

	if (creationSuccess == -1 ){
		log_printf(LOG_INFO, "error parsing file");
		exit(EXIT_FAILURE);
	}

	return metaFileInfo;
}

int startClient(struct torrent_t *metaFileInfo){
	int isDownloadedOrInvalid = downloadFile(metaFileInfo);

	return isDownloadedOrInvalid;
}

#endif
