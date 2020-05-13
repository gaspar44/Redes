#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "file_io.h"
#include "ttorrent.h"

void checkValidPortTest(char *supressWarning);
void checkInvalidPortTestError(char *supressWarning);
void checkReservedPortTestError(char *surpressWarning);
void allocateServerStructsTest(char *supressWarning);
void allocateServerStructsAlreadyBindedPortTestError(char *supressWarning);
void allocateServerStructsInvalidSocketTestError(char *supressWarning);
void allocateServerStructsNoSocketTest(char *supressWarning);
void checkHeaderReceivedFromClientTest(char *supressWarning);
void checkHeaderReceivedFromClientInvalidMagicNumberTestError(char *supressWarning);
void checkHeaderReceivedFromClientInvalidMsgTestError(char *supressWarning);
void checkHeaderReceivedFromServerTest(char *supressWarning);
void checkHeaderReceivedFromServerNotAvaliableBlockTest(char *supressWarning);
void checkHeaderReceivedFromServerIncorrectBlockTestError(char *supressWarning);
void checkHeaderReceivedFromServerInvalidMagicNumberTestError(char *supressWarning);



void checkValidPortTest(char *supressWarning) {
	(void) supressWarning;
	char *validPort = "1025";
	int parsedPort = checkValidPort(validPort);

	assert(parsedPort == 1025);
	printf("checkValidPortTest OK\n");
}

void checkInvalidPortTestError(char *supressWarning){
	(void) supressWarning;
	char *invalidPort = "-1";
	int parsedPort = checkValidPort(invalidPort);

	assert(parsedPort == -1);
	printf("checkInvalidPortTestError OK\n");
}

void checkReservedPortTestError(char *surpressWarning){
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

	printf("checkReservedPortTestError OK\n");
}

void allocateServerStructsTest(char *supressWarning){
	(void) supressWarning;
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("socket failed... allocateServerStructsTest failed");
		return;
	}

	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((uint16_t)9765);

	int err = allocateServerStructs(sockfd,servaddr);
	assert(err == 0);

	shutdown(sockfd, 2);
	close(sockfd);
	printf("allocateServerStructsTest OK\n");
}

void allocateServerStructsInvalidSocketTestError(char *supressWarning){
	(void) supressWarning;
	int sockfd = -1;
	struct sockaddr_in servaddr;

	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((uint16_t)9765);

	int err = allocateServerStructs(sockfd,servaddr);
	assert(err == -1);

	shutdown(sockfd, 2);
	close(sockfd);
	printf("allocateServerStructsInvalidSocketTestError OK\n");
}

void allocateServerStructsNoSocketTest(char *supressWarning){
	(void) supressWarning;
	int sockfd = 2;
	struct sockaddr_in servaddr;

	memset((char *)&servaddr,0,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((uint16_t)9765);

	int err = allocateServerStructs(sockfd,servaddr);
	assert(err == -1);

	shutdown(sockfd, 2);
	close(sockfd);
	printf("allocateServerStructsNoSocketTest OK\n");
}

void allocateServerStructsAlreadyBindedPortTestError(char *supressWarning){
	(void) supressWarning;
		int sockfd;
		struct sockaddr_in servaddr;

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			printf("socket failed... allocateServerStructsTest failed");
			return;
		}

		memset((char *)&servaddr,0,sizeof(servaddr));

		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons((uint16_t)80);

		int err = allocateServerStructs(sockfd,servaddr);
		assert(err == -1);

		shutdown(sockfd, 2);
		close(sockfd);
		printf("allocateServerStructsAlreadyBindedPortTestError OK\n");
}

void checkHeaderReceivedFromClientTest(char *supressWarning){
	(void) supressWarning;
	char *clientRequest = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(clientRequest, 0, RAW_MESSAGE_SIZE);

	memcpy(clientRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(clientRequest + sizeof(MAGIC_NUMBER),&MSG_REQUEST,sizeof(MSG_REQUEST));
	memcpy(clientRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	uint64_t block = checkHeaderReceivedFromClient(clientRequest);
	assert(block == blockNumber);
	free(clientRequest);

	printf("checkHeaderReceivedFromClientTest OK\n");
}

void checkHeaderReceivedFromClientInvalidMagicNumberTestError(char *supressWarning){
	(void) supressWarning;
	char *clientRequest = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(clientRequest, 0, RAW_MESSAGE_SIZE);
	uint32_t fakeMagicNumber = 0xde1c3231;

	memcpy(clientRequest, &fakeMagicNumber, sizeof(fakeMagicNumber));
	memcpy(clientRequest + sizeof(fakeMagicNumber),&MSG_REQUEST,sizeof(MSG_REQUEST));
	memcpy(clientRequest + sizeof(fakeMagicNumber) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	uint64_t block = checkHeaderReceivedFromClient(clientRequest);
	assert(block == ERROR_BLOCK);
	free(clientRequest);

	printf("checkHeaderReceivedFromClientInvalidMagicNumberTestError OK\n");
}

void checkHeaderReceivedFromClientInvalidMsgTestError(char *supressWarning) {
	(void) supressWarning;
	char *clientRequest = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(clientRequest, 0, RAW_MESSAGE_SIZE);

	memcpy(clientRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(clientRequest + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_OK,sizeof(MSG_RESPONSE_OK));
	memcpy(clientRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK), &blockNumber,sizeof(blockNumber));
	uint64_t block = checkHeaderReceivedFromClient(clientRequest);
	assert(block == ERROR_BLOCK);
	free(clientRequest);

	printf("checkHeaderReceivedFromClientInvalidMsgTestError OK\n");
}

void checkHeaderReceivedFromServerTest(char *supressWarning){
	(void) supressWarning;
	char *serverResponse = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(serverResponse, 0, RAW_MESSAGE_SIZE);

	memcpy(serverResponse, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_OK,sizeof(MSG_RESPONSE_OK));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	int block = checkHeaderReceivedFromServer(serverResponse,blockNumber);
	assert((uint64_t)block == blockNumber);
	free(serverResponse);
	printf("checkHeaderReceivedFromServerTest OK\n");
}

void checkHeaderReceivedFromServerNotAvaliableBlockTest(char *supressWarning){
	(void) supressWarning;
	char *serverResponse = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(serverResponse, 0, RAW_MESSAGE_SIZE);

	memcpy(serverResponse, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_NA,sizeof(MSG_RESPONSE_NA));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	int block = checkHeaderReceivedFromServer(serverResponse,blockNumber);
	assert(block == 2);
	free(serverResponse);

	printf("checkHeaderReceivedFromServerNotAvaliableTest OK\n");
}

void checkHeaderReceivedFromServerIncorrectBlockTestError(char *supressWarning){
	(void) supressWarning;
	char *serverResponse = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	memset(serverResponse, 0, RAW_MESSAGE_SIZE);

	memcpy(serverResponse, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER),&MSG_RESPONSE_OK,sizeof(MSG_RESPONSE_OK));
	memcpy(serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	int block = checkHeaderReceivedFromServer(serverResponse,2);
	assert(block == -1);
	free(serverResponse);

	printf("checkHeaderReceivedFromServerIncorrectBlockTestError OK\n");
}

void checkHeaderReceivedFromServerInvalidMagicNumberTestError(char *supressWarning){
	(void) supressWarning;
	char *serverResponse = (char*)malloc(RAW_MESSAGE_SIZE);
	uint64_t blockNumber = 0;
	uint32_t fakeMagicNumber = 0xde1c3231;
	memset(serverResponse, 0, RAW_MESSAGE_SIZE);

	memcpy(serverResponse, &fakeMagicNumber, sizeof(fakeMagicNumber));
	memcpy(serverResponse + sizeof(fakeMagicNumber),&MSG_RESPONSE_OK,sizeof(MSG_RESPONSE_OK));
	memcpy(serverResponse + sizeof(fakeMagicNumber) + sizeof(MSG_REQUEST), &blockNumber,sizeof(blockNumber));
	int block = checkHeaderReceivedFromServer(serverResponse,blockNumber);
	assert(block == -1);
	free(serverResponse);

	printf("checkHeaderReceivedFromServerInvalidMagicNumberTestError OK\n");
}


int main(int argc, char **argv) {
	(void) argc;
	(void) argv;
	set_log_level(LOG_NONE);

	checkValidPortTest(NULL);
	checkInvalidPortTestError(NULL);
	checkReservedPortTestError(NULL);
	allocateServerStructsTest(NULL);
	allocateServerStructsInvalidSocketTestError(NULL);
	allocateServerStructsNoSocketTest(NULL);
	allocateServerStructsAlreadyBindedPortTestError(NULL);
	checkHeaderReceivedFromClientTest(NULL);
	checkHeaderReceivedFromClientInvalidMagicNumberTestError(NULL);
	checkHeaderReceivedFromClientInvalidMsgTestError(NULL);
	checkHeaderReceivedFromServerTest(NULL);
	checkHeaderReceivedFromServerNotAvaliableBlockTest(NULL);
	checkHeaderReceivedFromServerIncorrectBlockTestError(NULL);
	checkHeaderReceivedFromServerInvalidMagicNumberTestError(NULL);

	return 0;

}
