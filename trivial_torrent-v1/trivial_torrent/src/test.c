#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "file_io.h"
#include "ttorrent.h"

void checkValidPortTest(char *supressWarning);
void checkInvalidPortTest(char *supressWarning);
void checkReservedPortTest(char *surpressWarning);
void getMetaFileInfoTest(char *metaFileName);

void checkValidPortTest(char *supressWarning) {
	(void) supressWarning;
	char *validPort = "1025";
	int parsedPort = checkValidPort(validPort);

	assert(parsedPort == 1025);
}

void checkInvalidPortTest(char *supressWarning){
	(void) supressWarning;
	char *invalidPort = "-1";
	int parsedPort = checkValidPort(invalidPort);

	assert(parsedPort == -1);
}

void checkReservedPortTest(char *surpressWarning){
	(void) surpressWarning;
	char *reserverdPort = "22";
	int parsedPort = checkValidPort(reserverdPort);
	assert(parsedPort == -1);

	reserverdPort = (char*) "0";
	parsedPort = checkValidPort(reserverdPort);
	assert(parsedPort == -1);

	reserverdPort = (char*) "1024";
	parsedPort = checkValidPort(reserverdPort);
	assert(parsedPort == -1);
}

void getMetaFileInfoTest(char *metaFileName){
	printf("%s\n",metaFileName);
}

int main(int argc, char **argv) {
	set_log_level(LOG_NONE);

	checkValidPortTest(NULL);
	checkInvalidPortTest(NULL);
	checkReservedPortTest(NULL);

	if (argc != 1){
		getMetaFileInfoTest(argv[1]);
	}

	return 0;

}
