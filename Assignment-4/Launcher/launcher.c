#include "../loader/loader.h"


int main(int argc,char*argv[]){

// we check here that if the arguments provided are lnot equal to 2 then we tell the user the usage
if(argc!=2){
        fprintf(stderr,"Usage: launch binary file , elf_file\n");
        exit(1);
    }

    // here we check the if the launcher binary file exists
    if(access(argv[0],F_OK)!=0){
fprintf(stderr,"error Launcher binary does not exist\n");
    exit(1);
    }


    // here we check if the binary file of test code created exists
    if(access(argv[1],F_OK)!=0){
        fprintf(stderr,"error test binary File does not exist\n");
        exit(1);
  }


// here we check if the launcher binary file has the permisiion to read
    
    
if(access(argv[0],R_OK)!=0){
        fprintf(stderr,"error Launcher binary is not readable\n");
        exit(1);
    }




// here we check that is the test binary file is readabele
  if(access(argv[1],R_OK)!=0){
      fprintf(stderr,"error test binary File is not readable\n");
    exit(1);

    }

    
// we check if we are able to open the file or not 
          int fd=open(argv[1],O_RDONLY);
        if(fd<0){
            perror("Error opening file");
            exit(1);
        }
        close(fd);

        printf("\nELF execution started\n");

// (c) Pass it to the loader for carrying out the loading/execution
    load_and_run_elf(argv);
    printf("ELF execution completed\n");

    // (d) Invoke the cleanup routine inside the loader
        loader_cleanup();

    exit(0);
}
