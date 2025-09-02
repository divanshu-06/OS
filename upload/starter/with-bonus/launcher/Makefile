all: ../bin/launch

#  -m2 tells to execute 32 bit architecture
# -L adds bin to linkers search directory for libraries
# -l tells linker to look for link with library named lib<name>.so
# -WL,<option> it tells to pass the options directy to the linker and see where the binary lives
# here $$ORIGIN is bin folder

#Compile the launch.c by linking it with the lib_simpleloader.so
../bin/launch:
	gcc -m32 launch.c -L../bin -l_simpleloader -Wl,-rpath='$$ORIGIN' -o ../bin/launch


#Provide the command for cleanup
clean:
	rm -f ../bin/launch

