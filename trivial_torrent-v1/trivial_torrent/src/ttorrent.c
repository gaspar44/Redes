// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>

// TODO: hey!? what is this?

// https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols

void createServer(char *portCandidate);
int createClient(struct torrent_t *metaInfoFile);
struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer);
int checkResponseHeader(char * serverResponse,uint64_t blockNumber);
int downloadFile(struct torrent_t *metaFileInfo,int sockfd);

static const uint32_t MAGIC_NUMBER = 0xde1c3230;

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13,
	RAW_RESPONSE_SIZE = 13
};

void createServer(char *portCandidate ) {
	int port = atoi(portCandidate);

	if (port == 0) {
		log_printf(LOG_INFO,"error: the %s is not a valid port\n",portCandidate);
		exit(EXIT_FAILURE);
	}

	else if (port >= 1 && port <= 1024) {
		log_printf(LOG_INFO, "error: the port %d is a reserved port\n",port);
		exit(EXIT_FAILURE);
	}

	log_printf(LOG_INFO,"Server mode (port %d)\n",port);

	//TODO: server stuff
}

int checkResponseHeader(char * serverResponse,uint64_t blockNumber){
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

int downloadFile(struct torrent_t *metaFileInfo,int sockfd){
	for (uint64_t blockNumber = 0; blockNumber < metaFileInfo->block_count; blockNumber++){
		char *blockToRequest = (char*)malloc(RAW_MESSAGE_SIZE);

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
			int validHeader = checkResponseHeader(serverResponse,blockNumber);

			if (validHeader == -1){
				log_printf(LOG_INFO, "error in header");
				blockNumber--;

				free(blockToRequest);
				free(serverResponse);
				continue;
			}

			if (validHeader == 2){
				free(blockToRequest);
				free(serverResponse);
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
	}

	return 0;
}

struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer){
	char * extension = strtok(metaFileToDownloadFromServer,".");
	char leftPartOfName[1024] = {0};
	strcpy(leftPartOfName,extension);
	strcat(leftPartOfName,".txt");
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
	}

	return metaFileInfo;
}

int createClient(struct torrent_t *metaFileInfo){
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_printf(LOG_INFO, "error opening socket. Exiting...\n");
		exit(EXIT_FAILURE);
	}

	log_printf(LOG_INFO,"Socket created successfully\n");
	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8080);

	if ( (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) == -1) {
		log_printf(LOG_INFO, "error connecting socket. Exiting...\n");
		exit(EXIT_FAILURE);
	}

	log_printf(LOG_INFO,"Socket connected successfully\n");

	int isDownloadedOrInvalid = downloadFile(metaFileInfo,sockfd);

//	log_printf(LOG_INFO, "file downloaded ;)");
	close(sockfd);
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
			createServer(argv[i + 1]);
		}
	}

	else {
		struct torrent_t metaFileInfo = getMetaFileInfo(argv[argc - 1]);
		int fileDownloaded =createClient(&metaFileInfo);

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
