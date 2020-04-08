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
void createClient(const struct torrent_t *metaInfoFile);
struct torrent_t getMetaFileInfo(char *metaFileToDownloadFromServer);

static const uint32_t MAGIC_NUMBER = 0xde1c3230;

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13 };

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

void createClient(const struct torrent_t *metaFileInfo){
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
	for (uint64_t blockNumber = 0; blockNumber < metaFileInfo->block_count; blockNumber++){
		void *blockToRequest[] = {MAGIC_NUMBER,MSG_REQUEST,blockNumber};//(char*) malloc(RAW_MESSAGE_SIZE);

//		memcpy(blockToRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
//		memcpy(blockToRequest + sizeof(MAGIC_NUMBER),&MSG_REQUEST,sizeof(MSG_REQUEST));
//		memcpy(blockToRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
		ssize_t error = send(sockfd,&blockToRequest,RAW_MESSAGE_SIZE,0);

		if (error < 0){
			log_printf(LOG_INFO, "error received from server");
			exit(EXIT_FAILURE);
		}

		struct block_t block;
		block.size = get_block_size(metaFileInfo, blockNumber);
		ssize_t readedBytes = recv(sockfd,block.data,sizeof(block),0);

		if ( readedBytes > 0) {
			log_printf(LOG_INFO, "data of block %ld received\n",blockNumber);
		}

		else {
			log_printf(LOG_INFO,"error received");
			exit(EXIT_FAILURE);
		}

//		unsigned char *md = malloc(block.size);
//		SHA256(block.data,block.size, md);
		//free(blockToRequest);
		//free(md);
	}
	shutdown(sockfd, 2);
	close(sockfd);
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
		createClient(&metaFileInfo);
	}
	/*int size = sizeof(my_msg); //get 

                char* buffer[size];
                memset(buffer, 0x00, size);

                memcpy(buffer, &my_msg, size);


                ssize_t messagee = send(sock_server, buffer, size, 0);
	*/

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
