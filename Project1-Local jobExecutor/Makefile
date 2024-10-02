# Makefile for jobExecutor program

# To BUILD the program, in tty: 
# 1) Type "make", for the separate compilation to take place

# To RUN the program, in tty:
# 2) Type ./jobCommander "command", where "command" can be a lot of things (check README)

# To CLEAN all the .ο and executable files, created after build/make, in tty:
# 3) Type "make clean"

# Automated extra utilities:
# "make start" (for easy start/initialization of the jobExecutorServer)
# "make exit" (for easy termination of the jobExecutorServer)
# "make ps" (for fast ckecking if any jobExecutorServer or jobCommander still runs on the background)

# 1) Build all executables together
all: jobCommander jobExecutorServer

jobCommander: jobCommander.o ExtraFunctions.o QueueFunctions.o
	gcc -Wall -Wextra -g3 -o $@ $^ 

jobCommander.o: jobCommander.c MyHeader.h
	gcc -Wall -Wextra -g3 -c jobCommander.c -o $@

jobExecutorServer: jobExecutorServer.o ExtraFunctions.o QueueFunctions.o
	gcc -Wall -Wextra -g3 -o $@ $^

jobExecutorServer.o: jobExecutorServer.c MyHeader.h
	gcc -Wall -Wextra -g3 -c jobExecutorServer.c -o $@

ExtraFunctions.o: ExtraFunctions.c MyHeader.h
	gcc -Wall -Wextra -g3 -c ExtraFunctions.c -o $@

QueueFunctions.o: QueueFunctions.c MyHeader.h
	gcc -Wall -Wextra -g3 -c QueueFunctions.c -o $@

# 3) Clean all files created after compilation/execution
clean:
	rm -f jobCommander jobCommander.o jobExecutorServer jobExecutorServer.o ExtraFunctions.o QueueFunctions.o jobExecutorServer.txt ClientsToSerFIFO ServerToClFIFO* running_jobs.txt queued_jobs.txt

# Other utilities/shortcuts:
start:
	./jobCommander start

exit:
	./jobCommander exit

ps:
	ps -aef | grep job

# (above, the $@ is the respective target, and the $^ are the respective depencencies)
# Extra gcc flags: -pedantic -Wformat -Wuninitialized -Wconversion -fsanitize=address -fsanitize=undefined
# Extra valgrind flags: -s --leak-check=full --track-origins=yes --trace-children=yes