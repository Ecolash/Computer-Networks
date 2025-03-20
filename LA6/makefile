CC = gcc
CFLAGS = -Wall

all: mysmtp_server mysmtp_client

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o mysmtp_server mysmtp_server.c

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o mysmtp_client mysmtp_client.c

server: mysmtp_server
	./mysmtp_server 2525

client: mysmtp_client
	./mysmtp_client 127.0.0.1 2525

clean:
	rm -f mysmtp_server mysmtp_client
