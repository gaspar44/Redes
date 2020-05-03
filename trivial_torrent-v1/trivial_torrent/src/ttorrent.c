// Trivial Torrent

// TODO: some includes here

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
int createServer(int portToUse,struct torrent_t* metafileInfo);
void waitForChildProccesses(int signal);
int allocateServerStructs(int sockFd,struct sockaddr_in servaddr);
int forkProcess(int newFileDescriptorToUse,struct torrent_t *metaFileInfo);
void recvMessageRequestFromClient(int socketFileDescriptor,char * serverRequestMessage);
uint64_t checkHeaderRecivedFromClient(char *clientRequest);
int sendResponseToClient(int socketFileDescriptor, char *blockToSend,uint64_t bytesToSendBack);


int startClient(struct torrent_t *metaInfoFile);
struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer,int isServer);
int checkHeaderRecivedFromServer(char * serverResponse,uint64_t blockNumber);
int downloadFile(struct torrent_t *metaFileInfo);
int createClientToServerConnection(struct torrent_t *metaFileInfo,int blockNumber);

static const uint32_t MAGIC_NUMBER = 0xde1c3230;

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;
static const uint64_t ERROR_BLOCK = 9999;

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

		if ( listen(sockfd, 100) == -1){
			log_printf(LOG_INFO, "error listening...\n");

			return -1;
		}

		return 0;
}

void waitForChildProccesses(int signal){
	(void) signal;
	//log_printf(LOG_DEBUG, "Signal %d recived\n",signal);
	pid_t pidToExit;
	int status;

	while(pidToExit != 0 && pidToExit != -1){
		pidToExit = waitpid(-1,&status,WNOHANG); // See man wait for details
	}
}

int forkProcess(int newFileDescriptorToUse,struct torrent_t *metaFileInfo){
	char *serverRequestMessage = (char*)malloc(RAW_MESSAGE_SIZE);


	while(1){
		memset(serverRequestMessage, 0, RAW_MESSAGE_SIZE);
		recvMessageRequestFromClient(newFileDescriptorToUse, serverRequestMessage);
		uint64_t errorOrBlockToSend = checkHeaderRecivedFromClient(serverRequestMessage);

		if (errorOrBlockToSend == ERROR_BLOCK)
			return EXIT_FAILURE;

		if (errorOrBlockToSend > metaFileInfo->block_count) {
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
				printf("forked client ;). Closing unnecessary stuff\n");
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

void recvMessageRequestFromClient(int socketFileDescriptor,char * serverRequestMessage) {
	ssize_t totalBytesReadedFromClient = 0;
	while (totalBytesReadedFromClient != RAW_MESSAGE_SIZE){
		ssize_t bytesReadedFromClient = recv(socketFileDescriptor,serverRequestMessage + totalBytesReadedFromClient,RAW_MESSAGE_SIZE,0);

		if (bytesReadedFromClient < 0){
			if (errno == EPIPE)
				return;
			sleep(1);
		}
		else{
			log_printf(LOG_INFO,"readed Bytes: %ld\n",bytesReadedFromClient);
			totalBytesReadedFromClient = totalBytesReadedFromClient + bytesReadedFromClient;
		}

	}
}

uint64_t checkHeaderRecivedFromClient(char *clientRequest){
	log_printf(LOG_INFO,"checking header...");
		uint32_t *requestMagicNumber = (uint32_t *) clientRequest;
		uint8_t *requestMessage = (uint8_t *)(clientRequest + sizeof(MAGIC_NUMBER));
		uint64_t *requestBlockNumer = (uint64_t *) (clientRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK));
		log_printf(LOG_INFO, "recived from client:\nMagicNumber:0x%08X\nrequestMessage: %ld\nblock: %d\n",*requestMagicNumber,*requestMessage,*requestBlockNumer);

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

	while (timeOut != 30 && (uint64_t)totalBytesSended != bytesToSendBack){
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

	return timeOut == 30 ? -1 : 0;
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
			int exited = createServer(port,&metaFileInfo);
			destroy_torrent(&metaFileInfo);

			if (exited == -1) {
				log_printf(LOG_INFO, "something terrible happend :(");
				exit(EXIT_FAILURE);
			}
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
