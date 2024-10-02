# Makefile for jobNetworkExecutor program

# To BUILD the program, in tty: 
# Type "make", for the separate compilation to take place

# To RUN the program, in tty:
# 1) Type ./jobExecutorServer [portNum] [bufferSize] [threadPoolSize]
# 2) Type ./jobCommander [serverName] [portNum] [jobCommanderInputCommand]

# To CLEAN all the .ο and executable files, created after compilation/execution, in tty:
# Type "make clean"


# Build all executables together
all: bin/jobCommander bin/jobExecutorServer

bin/jobCommander: build/jobCommander.o build/CommonFunctions.o
	gcc -Wall -Wextra -g3 -o $@ $^ 

build/jobCommander.o: src/jobCommander.c include/CommonInterface.h
	gcc -Wall -Wextra -g3 -c src/jobCommander.c -o $@

bin/jobExecutorServer: build/jobExecutorServer.o build/CommonFunctions.o build/Worker.o build/Controller.o
	gcc -Wall -Wextra -g3 -o $@ $^ -pthread

build/jobExecutorServer.o: src/jobExecutorServer.c include/CommonInterface.h include/ServerInterface.h
	gcc -Wall -Wextra -g3 -c src/jobExecutorServer.c -o $@ -pthread

build/CommonFunctions.o: src/CommonFunctions.c include/CommonInterface.h
	gcc -Wall -Wextra -g3 -c src/CommonFunctions.c -o $@

build/Worker.o: src/Worker.c include/ServerInterface.h src/CommonFunctions.c include/CommonInterface.h
	gcc -Wall -Wextra -g3 -c src/Worker.c -o $@ -pthread

build/Controller.o: src/Controller.c include/ServerInterface.h src/CommonFunctions.c include/CommonInterface.h
	gcc -Wall -Wextra -g3 -c src/Controller.c -o $@ -pthread

# Clean all files created after compilation/execution
clean:
	rm -f bin/jobCommander build/jobCommander.o bin/jobExecutorServer build/jobExecutorServer.o build/CommonFunctions.o build/Worker.o build/Controller.o


# (above, the $@ is the respective target, and the $^ are the respective depencencies)
# Extra gcc flags: -pedantic -Wformat -Wuninitialized -Wconversion -fsanitize=address -fsanitize=undefined -O3
# Extra valgrind flags: -s --leak-check=full --track-origins=yes --trace-children=yes