all: make_objects compile

make_objects:
	gcc -c sucklessg.c jsmn.c

compile:
	gcc sucklessg.o jsmn.o -lssl -lcrypto

clean:
	rm *.o *.out
