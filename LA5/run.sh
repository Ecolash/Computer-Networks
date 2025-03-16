#!/bin/bash
gcc -o server server.c 
gcc -o client client.c

xterm -T "SERVER" -fa "Monospace" -fs 11 -bg black -fg magenta -geometry 80x24+0+0 -b 2 -e bash -c "./server; exec bash" &
sleep 2

xterm -T "CLIENT-1" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 80x24+800+0 -b 2 -e bash -c "./client; exec bash" &
xterm -T "CLIENT-2" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 80x24+0+550 -b 2 -e bash -c "./client -n 4; exec bash" &
xterm -T "CLIENT-3" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 80x24+800+550 -b 2 -e bash -c "./client; exec bash" &

echo "System started: 1 server and 3 clients"