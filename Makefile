all: compile

compile:
	gcc -pedantic client.c hashtable.c jsmn.c networking.c prime.c JSONprocessing.c -lssl -lcrypto -lm -lform -lncurses

clean:
	rm *.o *.out
