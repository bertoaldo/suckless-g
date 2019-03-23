all: compile

compile:
	gcc -pedantic -o suckless-g client.c hashtable.c jsmn.c networking.c prime.c JSONprocessing.c -lssl -lcrypto -lm -lformw -lncursesw

clean:
	rm *.o *.out
