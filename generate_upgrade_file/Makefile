CC = gcc

generate_upgrade_file: generate_upgrade_file.o
	$(CC) -o $@ $<

generate_upgrade_file.o: generate_upgrade_file.c
	$(CC) -c $<

clean:
	-rm -rf generate_upgrade_file *.o
