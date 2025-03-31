CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGETS = cldp_client cldp_server

all: $(TARGETS)

cldp_client: cldp_client.c
	$(CC) $(CFLAGS) -o cldp_client cldp_client.c

cldp_server: cldp_server.c
	$(CC) $(CFLAGS) -o cldp_server cldp_server.c

runserver: cldp_server
	sudo ./cldp_server

runclient: cldp_client
	sudo ./cldp_client

clean:
	rm -f $(TARGETS)

.PHONY: all runserver runclient clean