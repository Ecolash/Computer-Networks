#!/bin/bash
gcc -o server server.c 
gcc -o client client.c

xterm -T "SERVER" -fa "Monospace" -fs 11 -bg black -fg green -geometry 60x24+0+0 -b 2 -e bash -c "./server; exec bash" &
sleep 2

xterm -T "CLIENT-1" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 60x24+625+0 -b 2 -e bash -c "./client -P 0.1; exec bash" &
xterm -T "CLIENT-2" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 60x24+1200+0 -b 2 -e bash -c "./client -P 0.2; exec bash" &
xterm -T "CLIENT-3" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 60x24+0+550 -b 2 -e bash -c "./client; exec bash" &
xterm -T "CLIENT-4" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 60x24+625+550 -b 2 -e bash -c "./client -n 6; exec bash" &
xterm -T "CLIENT-5" -fa "Monospace" -fs 11 -bg black -fg cyan -geometry 60x24+1200+550 -b 2 -e bash -c "./client -P 0.3; exec bash" &

echo "System started: 1 server and 3 clients"