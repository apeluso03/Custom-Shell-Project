CC=gcc

mysh: shell.o getpath.o linkedlist.o watchuser.o main.c 
	$(CC) -g main.c shell.o getpath.o linkedlist.o watchuser.o -o mysh -lpthread

linkedlist.o: linkedlist.c linkedlist.h
	$(CC) -g -c linkedlist.c

watchuser.o: watchuser.c watchuser.h
	$(CC) -g -c watchuser.c

shell.o: shell.c shell.h
	$(CC) -g -c shell.c

getpath.o: getpath.c getpath.h
	$(CC) -g -c getpath.c

clean:
	rm -rf shell.o getpath.o linkedlist.o watchuser.o mysh
