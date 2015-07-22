#include <cstdio>
#include <unistd.h>

#include "../src/singleinstance.h"

using namespace LibSingleInstance;

volatile bool running = true;

void callback(void *, int argc, char **argv) {
	for(int i = 1; i < argc; ++i)
		printf("%s ", argv[i]);
	printf("\n");
	if(argc == 2 && std::string(argv[1]) == "--stop") running = false;
}

int main(int argc, char **argv) {
	try {
		SingleInstanceAuto handle("test", argc, argv, &callback);
		unsigned int cnt = 0;

		while(running) {
			printf("%u\r", cnt);
			fflush(stdout);
			sleep(1);
			++cnt;
		}
	} catch(SingleInstance::Exists) {
	} catch(const std::runtime_error &err) {
		fprintf(stderr, "Error: %s\n", err.what());
		return 1;
	}
	return 0;
}
