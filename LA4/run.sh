#!/bin/bash

SRC_IP="127.0.0.1"
DEST_IP="127.0.0.1"
SRC_PORT=4030
DEST_PORT=4031

echo "Removing existing shared memory segments"
ipcs -m | awk 'NR>3 {print $2}' | grep -E '^[0-9]+$' | xargs -r -I {} ipcrm -m {}

echo "Checking and unbinding ports if in use"
for port in $SRC_PORT $DEST_PORT; do
    fuser -k ${port}/udp 2>/dev/null
    fuser -k ${port}/tcp 2>/dev/null
done

echo "Compiling ksocket.c and creating static library"
rm -f *.o libksocket.a initksocket user1 user2
rm -f new_$DEST_PORT.txt
gcc -Wall -c ksocket.c
ar rcs libksocket.a ksocket.o

echo "Compiling initksocket.c"
gcc -Wall -c initksocket.c
gcc -Wall -o initksocket initksocket.o -L. -lksocket

echo "Compiling user1.c"
gcc -Wall -c user1.c
gcc -Wall -o user1 user1.o -L. -lksocket

echo "Compiling user2.c"
gcc -Wall -c user2.c
gcc -Wall -o user2 user2.o -L. -lksocket

echo "Opening terminals and executing programs"
xterm -T "INIT_KSOCKET" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 80x24+0+550 -b 2 -e ./initksocket &
sleep 1
xterm -T "USER2" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 90x25+1000+0 -b 2 -e ./user2 $SRC_IP $SRC_PORT $DEST_IP $DEST_PORT &
sleep 1
xterm -T "USER1" -fa "Monospace" -fs 12 -bg black -fg cyan -geometry 90x25+0+0 -b 2 -e ./user1 $DEST_IP $DEST_PORT $SRC_IP $SRC_PORT &
