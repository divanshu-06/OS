# invoke make inside following directories and in this order: loader, launch, fib
# Build loader shared object file
# Link launcher with the shared object file created and make a final file
#move the lib_simpleloader.so and launch binaries inside bin directory

all:
	$(MAKE) -C loader
	$(MAKE) -C launcher
	$(MAKE) -C test
	

	
# make test program binary file 
test:
	$(MAKE) -C test


#Provide the command for cleanup
# Cleanup the files created on execution
clean:
	rm -f bin/* test/fib
	$(MAKE) -C loader clean
	$(MAKE) -C launch clean
	$(MAKE) -C test clean

# clean the test folder binary file

test-clean:
	$(MAKE) -C test clean

