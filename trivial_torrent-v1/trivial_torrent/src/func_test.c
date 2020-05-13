#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "file_io.h"
#include "ttorrent.h"

void sendRequestTest(struct torrent_t metaFile);
void sendRequestInvalidBlockTest(struct torrent_t metaFile);

void sendRequestTest(struct torrent_t metaFile){
	uint64_t blockNumberToRequest = 0;
	int sockfd = createClientToServerConnection(&metaFile, (int)blockNumberToRequest);
	assert(sockfd != -1);
	char *blockToRequest = (char*)malloc(RAW_MESSAGE_SIZE);

	memcpy(blockToRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(blockToRequest + sizeof(MAGIC_NUMBER),&MSG_REQUEST,sizeof(MSG_REQUEST));
	memcpy(blockToRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumberToRequest,sizeof(blockNumberToRequest));;

	ssize_t error = send(sockfd,blockToRequest,RAW_MESSAGE_SIZE,0);

	if (error < 0){
		printf("sendRequestTest failed\n");
		return;
	}

	struct block_t block;
	block.size = get_block_size(&metaFile, blockNumberToRequest);
	char *serverResponse = malloc(RAW_RESPONSE_SIZE + block.size);
	ssize_t totalReadedBytes = 0;

	while ((uint64_t)totalReadedBytes != RAW_RESPONSE_SIZE + block.size){
		ssize_t partialReadedBytes = recv(sockfd,serverResponse + totalReadedBytes,RAW_RESPONSE_SIZE + block.size,0);
		log_printf(LOG_INFO,"readed Bytes: %ld\n",partialReadedBytes);

		if (partialReadedBytes < 0) {
			printf("sendRequestTest failed\n");
			return;
		}

		else {
			totalReadedBytes = totalReadedBytes + partialReadedBytes;

			if (totalReadedBytes >= 13){
				int validHeader = checkHeaderReceivedFromServer(serverResponse,blockNumberToRequest);
				assert(validHeader == 0);
			}
		}
	}

	uint8_t * blockData = (uint8_t *) (serverResponse + sizeof(MAGIC_NUMBER) + sizeof(MSG_RESPONSE_OK) + sizeof(blockNumberToRequest));
	memcpy(block.data, blockData, block.size);

	int isValid = store_block(&metaFile, blockNumberToRequest, &block);
	assert(isValid == 0);

	close(sockfd);
	free(blockToRequest);
	free(serverResponse);
	printf("sendRequestTest OK\n");
}

void sendRequestInvalidBlockTest(struct torrent_t metaFile){
	uint64_t blockNumberToRequest = metaFile.block_count;
	int sockfd = createClientToServerConnection(&metaFile, 0);
	assert(sockfd != -1);
	char *blockToRequest = (char*)malloc(RAW_MESSAGE_SIZE);

	memcpy(blockToRequest, &MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
	memcpy(blockToRequest + sizeof(MAGIC_NUMBER),&MSG_REQUEST,sizeof(MSG_REQUEST));
	memcpy(blockToRequest + sizeof(MAGIC_NUMBER) + sizeof(MSG_REQUEST), &blockNumberToRequest,sizeof(blockNumberToRequest));;

	ssize_t error = send(sockfd,blockToRequest,RAW_MESSAGE_SIZE,0);

	if (error < 0){
		printf("sendRequestInvalidBlockTest failed\n");
		return;
	}

	char *serverResponse = malloc(RAW_RESPONSE_SIZE);
	ssize_t totalReadedBytes = 0;

	while ((uint64_t)totalReadedBytes != RAW_RESPONSE_SIZE){
		ssize_t partialReadedBytes = recv(sockfd,serverResponse + totalReadedBytes,RAW_RESPONSE_SIZE,0);
		log_printf(LOG_INFO,"readed Bytes: %ld\n",partialReadedBytes);

		if (partialReadedBytes < 0) {
			printf("sendRequestInvalidBlockTest failed\n");
			return;
		}

		else {
			totalReadedBytes = totalReadedBytes + partialReadedBytes;
		}
	}

	int validHeader = checkHeaderReceivedFromServer(serverResponse,blockNumberToRequest);
	assert(validHeader == 2);

	close(sockfd);
	free(blockToRequest);
	free(serverResponse);
	printf("sendRequestInvalidBlockTest OK\n");
}


int main(int argc, char **argv) {
	set_log_level(LOG_NONE);

	struct torrent_t metaFile = getMetaFileInfo(argv[argc -1],0);
	sendRequestTest(metaFile);
	sendRequestInvalidBlockTest(metaFile);
	destroy_torrent(&metaFile);

	return 0;
}
