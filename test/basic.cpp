#include "singleinstance.h"

#include <cstdio>

using namespace LibSingleInstance;

int main(int argc, char **argv) {
	try {
		SingleInstance instance("test", argc, argv);
		printf("I'm the first instance\n");
		while(1) {
			int argc;
			char **argv;
			if(instance.check(&argc, &argv, true)) {
				printf("New instance: ");
				for(int i = 1; i < argc; ++i) printf("%s ", argv[i]);
				printf("\n");
				instance.pop();
			}
		}
	} catch(SingleInstance::Exists) {
		printf("Another instance already running\n");
	}
}
