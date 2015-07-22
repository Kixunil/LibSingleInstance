#include "singleinstance.h"

#include <signal.h>

#include <cstdio>

using namespace LibSingleInstance;

class Signal {
	public:
		Signal(int signum, sighandler_t handler) : mSignum(signum) {
			struct sigaction sa;
			sa.sa_handler = handler;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;

			sigaction(signum, &sa, &mOldSa);
		}

		~Signal() {
			sigaction(mSignum, &mOldSa, NULL);
		}
	private:
		int mSignum;
		struct sigaction mOldSa;
};

SingleInstance *singleInstance = NULL;

void sigHandler(int) {
	if(singleInstance) singleInstance->stop();
}

int main(int argc, char **argv) {
	try {
		SingleInstance instance("test", argc, argv);

		singleInstance = &instance;
		// Signals will be reset at end of the scope
		Signal sigint(SIGINT, &sigHandler);
		Signal sigterm(SIGTERM, &sigHandler);

		printf("I'm the first instance\n");

		while(1) {
			int argc;
			char **argv;
			if(instance.check(&argc, &argv, true)) {
				printf("New instance: ");
				for(int i = 1; i < argc; ++i) printf("%s ", argv[i]);
				printf("\n");
				if(argc == 2 && std::string(argv[1]) == "--stop") break;
				instance.pop();
			} else break; // in case of interruption
		}
	} catch(SingleInstance::Exists) {
		printf("Another instance already running\n");
	}
	return 0;
}
