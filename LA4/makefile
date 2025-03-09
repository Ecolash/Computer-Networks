
# COMMANDS:

# user 1 and user 3 are the senders
# user 2 and user 4 are the receivers
# user 1 -----> user 2 (port 4031 to 4030)
# user 3 -----> user 4 (port 4033 to 4032)

# - `make` or `make all`: Build all components.
# - `make socket`: Initialize the ksocket module.
# - `make run1` / `make run2`: Run `user1` and `user2` (call on seperate terminals).
# - `make run3` / `make run4`: Run `user3` and `user4` (call on seperate terminals).
# - `make terminal`: Launch all users in separate xterm windows (requirement: xterm).
# - `make clean`: Remove compiled binaries and temporary files.

CC = gcc
CFLAGS = -Wall
LIBRARY = libksocket.a

SRC_IP = 127.0.0.1
DEST_IP = 127.0.0.1
PORT1 = 4030
PORT2 = 4031
PORT3 = 4032
PORT4 = 4033

all: $(LIBRARY) initksocket user1 user2

make socket:
	./initksocket

run1: user1
	./user1 $(DEST_IP) $(PORT2) $(SRC_IP) $(PORT1)

run2: user2
	./user2 $(SRC_IP) $(PORT1) $(DEST_IP) $(PORT2)

run3: user1
	./user1 $(DEST_IP) $(PORT4) $(SRC_IP) $(PORT3)

run4: user2
	./user2 $(SRC_IP) $(PORT3) $(DEST_IP) $(PORT4)

$(LIBRARY): ksocket.o
	ar rcs $(LIBRARY) ksocket.o

user1: user1.o $(LIBRARY)
	$(CC) $(CFLAGS) -o user1 user1.o -L. -lksocket

user1.o: user1.c ksocket.h
	$(CC) $(CFLAGS) -c user1.c

user2: user2.o $(LIBRARY)
	$(CC) $(CFLAGS) -o user2 user2.o -L. -lksocket

user2.o: user2.c ksocket.h
	$(CC) $(CFLAGS) -c user2.c

initksocket: initksocket.o $(LIBRARY)
	$(CC) $(CFLAGS) -o initksocket initksocket.o -L. -lksocket -pthread

initksocket.o: initksocket.c ksocket.h
	$(CC) $(CFLAGS) -c initksocket.c

ksocket.o: ksocket.c ksocket.h
	$(CC) $(CFLAGS) -c ksocket.c

clean:
	rm -f *.o user1 user2 $(LIBRARY) initksocket new_$(PORT2).txt new_$(PORT4).txt

terminal: all
	xterm -T "INIT_KSOCKET" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 80x24+0+550 -b 2 -e ./initksocket & sleep 1; 
	xterm -T "USER2" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 90x25+1000+0 -b 2 	 -e ./user2 $(SRC_IP) $(PORT1) $(DEST_IP) $(PORT2) &
	xterm -T "USER1" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 90x25+0+0 -b 2 	 -e ./user1 $(DEST_IP) $(PORT2) $(SRC_IP) $(PORT1); 
