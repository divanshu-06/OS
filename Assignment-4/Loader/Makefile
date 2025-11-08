# here we are linking launcher.c we lib_simpleloader.so and a final relocatnble file launch
# shred flag tells that we are building a shared library
# -m2 tell the compiler to execute 32 bit architecture

all: 
	gcc -m32 -shared loader.c -o ../bin/lib_simpleloader.so

clean:
	rm -f ../bin/lib_simpleloader.so


