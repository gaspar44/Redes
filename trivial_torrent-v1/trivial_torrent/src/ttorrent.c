
// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// TODO: hey!? what is this?

// https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols

void createServer(char *portCandidate);
void createClient(char *metaFileToDownloadFromServer);

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

void createClient(char *metaFileToDownloadFromServer){
	char * extension = strtok(metaFileToDownloadFromServer,".");
	char leftPartOfName[1024] = {0};
	strcpy(leftPartOfName,extension);
	strcat(leftPartOfName,".txt");
	extension = strtok(NULL,".");
	metaFileToDownloadFromServer = strcat(metaFileToDownloadFromServer,".ttorrent");


	printf("parte izquierda %s\n",leftPartOfName);

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
		createClient(argv[argc - 1]);
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
