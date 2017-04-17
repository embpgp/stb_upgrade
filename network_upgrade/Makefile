CROSS_CC = mipsel-linux-uclibc-gcc
CC = gcc
ALL: client server

client: client.o
	$(CROSS_CC) -o $@ $<
server: server.o
	$(CC)  -o $@ $< -lpthread
	
client.o: client.c
	$(CROSS_CC) -c $<

server.o: server.c
	$(CC) -c $<
clean:
	-rm -rf client server *.o
