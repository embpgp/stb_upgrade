CROSS_CC = mipsel-linux-uclibc-gcc
CC = gcc
ALL: send recv

recv: recv.o
	$(CROSS_CC) -o $@ $< 
send: send.o
	$(CC)  -o $@ $<  
	
recv.o: recv.c
	$(CROSS_CC) -c $< 

send.o: send.c
	$(CC) -c $< 
clean:
	-rm -rf recv send *.o
