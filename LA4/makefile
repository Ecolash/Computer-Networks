# Makefile for ksocket project

# Usage:
# - make all: Compiles ksocket.c, user1.c, and user2.c, and creates the executables user1 and user2.
# - make compile: Compiles only ksocket.c and creates the static library libksocket.a.
# - make run: Runs user1 and user2 in separate xterm terminals.
# - make clean: Removes all generated object files, executables, and temporary files.

SRC_IP=127.0.0.1
DEST_IP=127.0.0.1
SRC_PORT=4030
DEST_PORT=4031

all: compile run

compile: libksocket.a user1 user2 

libksocket.a: ksocket.c
	gcc -Wall -c ksocket.c
	ar rcs libksocket.a ksocket.o

user1: user1.c libksocket.a
	gcc -Wall -c user1.c
	gcc -Wall -o user1 user1.o -L. -lksocket

user2: user2.c libksocket.a
	gcc -Wall -c user2.c
	gcc -Wall -o user2 user2.o -L. -lksocket

run: compile
	rm -f new_$(SRC_PORT).txt
	xterm -T "USER2" -fa "Monospace" -fs 10 -bg black -fg cyan -geometry 80x24+1000+0 -b 2 -e ./user2 $(SRC_IP) $(SRC_PORT) $(DEST_IP) $(DEST_PORT) &
	xterm -T "USER1" -fa "Monospace" -fs 10 -bg black -fg cyan -geometry 80x24+0+0 -b 2 -e ./user1 $(DEST_IP) $(DEST_PORT) $(SRC_IP) $(SRC_PORT) &

clean:
	rm -f *.o libksocket.a user1 user2 new_$(SRC_PORT).txt
