// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>

// TODO: hey!? what is this?

// https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols

int checkValidPort(char *portCandidate);
int createServer(int portToUse,struct torrent_t metafileInfo);
int allocateServerStructs(int sockFd,struct sockaddr_in servaddr);
uint64_t checkHeaderRecivedFromClient(char *clientRequest);


int startClient(struct torrent_t *metaInfoFile);
struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer,int isServer);
int checkHeaderRecivedFromServer(char * serverResponse,uint64_t blockNumber);
int downloadFile(struct torrent_t *metaFileInfo);
int createClientToServerConnection(struct torrent_t *metaFileInfo,int blockNumber);

static const uint32_t MAGIC_NUMBER = 0xde1c3230;

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13,
	RAW_RESPONSE_SIZE = 13
};

int checkValidPort(char *portCandidate) {
	int port = atoi(portCandidate);

	if (port == 0) {
		log_printf(LOG_INFO,"error: the %s is not a valid port\n",portCandidate);
		return -1;
	}

	else if (port >= 1 && port <= 1024) {
		log_printf(LOG_INFO, "error: the port %d is a reserved port\n",port);
		return -1;
	}

	log_printf(LOG_INFO,"Server mode (port %d)\n",port);
	return port;
}

int allocateServerStructs(int sockfd,struct sockaddr_in servaddr) {
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1){ // Necessary for stop address already in use after each test
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

		if ( listen(sockfd, 100) == -1){
			log_printf(LOG_INFO, "error listening...\n");

			return -1;
		}

		log_printf(LOG_INFO, "Listen to connections now\n");

		if (fcntl(sockfd, F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1){ // Necessary for non-blocking
			log_printf(LOG_INFO, "error fcntl 2\n");
			return -1;
		}

		return 0;
}

int createServer(int portToUse,struct torrent_t metaFileInfo){

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
		int newFileDescriptorToUse = accept(sockfd, NULL, NULL);
		printf("%d\n",newFileDescriptorToUse);

		if (newFileDescriptorToUse == -1){
			if (errno == EWOULDBLOCK || errno == EAGAIN){
				sleep(1);
			}

			else {
				log_printf(LOG_INFO, "error while arriving connections\n");
				return -1;
			}
		}

		char *serverRequestMessage = (char*)malloc(RAW_MESSAGE_SIZE);
		memset(serverRequestMessage, 0, RAW_MESSAGE_SIZE);

		ssize_t totalBytesReadedFromClient = 0;

		if (newFileDescriptorToUse != -1){
			while (totalBytesReadedFromClient != RAW_MESSAGE_SIZE){
				ssize_t bytesReadedFromClient = recv(newFileDescriptorToUse,serverRequestMessage + totalBytesReadedFromClient,RAW_MESSAGE_SIZE,0);

				if (bytesReadedFromClient == -1)
					sleep(1);
				else
					totalBytesReadedFromClient = totalBytesReadedFromClient + bytesReadedFromClient;
			}

			uint64_t errorOrBlockToSend = checkHeaderRecivedFromClient(serverRequestMessage);

			if (errorOrBlockToSend == 9999)
				return -1;

			if (errorOrBlockToSend > metaFileInfo.block_count) {
				char *responseBlock = (char*)malloc(RAW_MESSAGE_SIZE);
				memcpy(responseBlock, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
				memcpy(responseBlock + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_NA,sizeof(MSG_RESPONSE_NA));
				memcpy(responseBlock + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &errorOrBlockToSend,sizeof(errorOrBlockToSend));

				ssize_t error = send(sockfd,responseBlock,RAW_MESSAGE_SIZE,0);

				if (error < 0)
					log_printf(LOG_INFO, "error sending response: %d\n",errno);

				free(responseBlock);
			}
		}


		char prueba2[] = "er Huevo\n";
		send(newFileDescriptorToUse,prueba2,sizeof(prueba2),0);
		printf("%s\n",serverRequestMessage);

		close(newFileDescriptorToUse);
		free(serverRequestMessage);
	}

	return 0;
}

uint64_t checkHeaderRecivedFromClient(char *clientRequest){
	log_printf(LOG_INFO,"checking header...");
		uint32_t *requestMagicNumber = (uint32_t *) clientRequest;
		uint8_t *requestMessage = (uint8_t *)(clientRequest + sizeof(MAGIC_NUMBER));
		uint64_t *requestBlockNumer = (uint64_t *) (clientRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK));


		if (*requestMessage != 0){
			log_printf(LOG_INFO, "invalid message from client\n");
			return 9999;
		}

		if (*requestMagicNumber != MAGIC_NUMBER){
			log_printf(LOG_INFO, "invalid Magic number from client");
			return 9999;
		}

		log_printf(LOG_INFO, "Valid header!");
		printf("%ld\n",*requestBlockNumer);

		return *requestBlockNumer;
}

int checkHeaderRecivedFromServer(char * serverResponse,uint64_t blockNumber){
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
		log_printf(LOG_INFO, "invalid Magic number from server");
		return -1;
	}

	if (*responseBlockNumer != blockNumber){
		log_printf(LOG_INFO, "invalid block");
		return -1;
	}

	log_printf(LOG_INFO, "Valid header!");
	return 0;
}

int createClientToServerConnection(struct torrent_t *metaFileInfo,int blockNumber) {
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_printf(LOG_INFO, "error opening socket. Exiting...\n");
		exit(EXIT_FAILURE);
	}

	log_printf(LOG_INFO,"Socket created successfully\n");
	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = *(uint32_t*)metaFileInfo->peers[blockNumber].peer_address;
	servaddr.sin_port = metaFileInfo->peers[blockNumber].peer_port;

	if ( (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) == -1) {
		log_printf(LOG_INFO, "error connecting socket. Exiting...\n");
		exit(EXIT_FAILURE);
	}

	log_printf(LOG_INFO,"Socket connected successfully\n");

	return sockfd;
}

int downloadFile(struct torrent_t *metaFileInfo){
	for (uint64_t blockNumber = 0; blockNumber < metaFileInfo->block_count; blockNumber++){
		char *blockToRequest = (char*)malloc(RAW_MESSAGE_SIZE);
		int sockfd = createClientToServerConnection(metaFileInfo, (int)blockNumber);

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
			ssize_t partialReadedBytes = recv(sockfd,serverResponse,RAW_RESPONSE_SIZE + block.size,0);

			if (partialReadedBytes < 0) {
				log_printf(LOG_INFO, "error reading from server");
				exit(EXIT_FAILURE);
			}

			else {
				totalReadedBytes = totalReadedBytes + partialReadedBytes;
				if ((uint64_t)totalReadedBytes == RAW_RESPONSE_SIZE + block.size){
					break;
				}
			}
		}
			int validHeader = checkHeaderRecivedFromServer(serverResponse,blockNumber);

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
				return 1;
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
	char * extension = strtok(metaFileToDownloadFromServer,".");
	char leftPartOfName[1024] = {0};
	strcpy(leftPartOfName,extension);

	if (isServer){
		FILE * fileToCheck;

		if ((fileToCheck = fopen(leftPartOfName, "r"))){
			fclose(fileToCheck);
		}

		else {
			log_printf(LOG_INFO, "file %s to use not found\n",leftPartOfName);
			exit(EXIT_FAILURE);
		}
	}

	extension = strtok(NULL,".");
	metaFileToDownloadFromServer = strcat(metaFileToDownloadFromServer,".ttorrent");

	if (extension == NULL) {
		log_printf(LOG_INFO,"ERROR: NO EXTENSION FOUND");
		exit(EXIT_FAILURE);
	}

	else if (strcmp(extension,"ttorrent") != 0){
		log_printf(LOG_INFO,"ERROR: Invalid torrent file extension");
		exit(EXIT_FAILURE);
	}

	struct torrent_t metaFileInfo;

	int creationSuccess = create_torrent_from_metainfo_file(metaFileToDownloadFromServer,&metaFileInfo,leftPartOfName);

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

/**
 * Main function.
 */
int main(int argc, char **argv) {

	set_log_level(LOG_DEBUG);

	log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "J. DOE and J. DOE");

	if (argc != 4 && argc != 2){
		log_printf(LOG_INFO,"ERROR: Usage is: ttorrent [-l port] file.ttorrent\n");
		exit(EXIT_FAILURE);
	}

	else if (argc == 4){
		int foundServerPortFlag = 0;
		int i;
		for (i = 1; i < argc;i++){

			if (strcmp("-l",argv[i]) == 0){
				foundServerPortFlag = 1;
				break;
			}
		}

		if (foundServerPortFlag == 1) {
			int port;

			if ((port = checkValidPort(argv[i + 1])) == -1){
				exit(EXIT_FAILURE);
			}

			struct torrent_t metaFileInfo = getMetaFileInfo(argv[argc - 1],1);
			createServer(port,metaFileInfo);
		}
	}

	else {
		struct torrent_t metaFileInfo = getMetaFileInfo(argv[argc - 1],0);
		int fileDownloaded = startClient(&metaFileInfo);

		if (fileDownloaded == 0){
			log_printf(LOG_INFO, "File downloaded :)");
		}

		else if (fileDownloaded == 1){
			log_printf(LOG_INFO,"file not downloaded");
		}

		destroy_torrent(&metaFileInfo);
	}

	// ==========================================================================
	// Parse command line
	// ==========================================================================

	// TODO: some magical lines of code here that call other functions and do various stuff.

	// The following statements most certainly will need to be deleted at some point...
	(void) argc;
	(void) argv;
	(void) MAGIC_NUMBER;
	(void) MSG_REQUEST;
	(void) MSG_RESPONSE_NA;
	(void) MSG_RESPONSE_OK;

	return 0;
}
