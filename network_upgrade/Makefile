CROSS_CC = mipsel-linux-uclibc-gcc
CC = gcc
ALL: client server

client: client.o
	$(CROSS_CC) -o $@ $<
server: server.o
	$(CC)  -o $@ $< -lpthread -g 
	
client.o: client.c
	$(CROSS_CC) -c $<

server.o: server.c
	$(CC) -c $< -g
clean:
	-rm -rf client server *.o
