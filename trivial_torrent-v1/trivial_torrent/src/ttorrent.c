// Trivial Torrent

// TODO: some includes here
#include <stdlib.h>
#include "logger.h"
#include "ttorrent.h"

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
		int flagParsed = 0;
	    while ((flagParsed = getopt(argc, argv, "l")) != -1) {
	        switch (flagParsed) {
	        case 'l': foundServerPortFlag = 1; break;
	        default:
	        	log_printf(LOG_INFO,"ERROR: Usage is: ttorrent [-l port] file.ttorrent\n");
	            exit(EXIT_FAILURE);
	        }
	    }

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

		else if (fileDownloaded == -1){
			log_printf(LOG_INFO,"file not downloaded");
		}

		destroy_torrent(&metaFileInfo);
	}

	return 0;
}
