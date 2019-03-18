all: compile

compile:
	gcc client.c hashtable.c jsmn.c networking.c prime.c JSONprocessing.c -lssl -lcrypto -lm -lncurses

clean:
	rm *.o *.out
