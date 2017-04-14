CC = mipsel-linux-uclibc-gcc
#CC = gcc
flash: flash.o
	$(CC) -o $@ $<

flash.o: flash.c
	$(CC) -c $<

clean:
	-rm -rf flash *.o
