#include "loader.h"

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

void *seg_addrs[128];
size_t seg_sizes[128];
int seg_count = 0;

/*
 * release memory and other cleanups
 */
void loader_cleanup(){
    // we unmap each loaded segment, checks it is not NULL before unmapping
    if(ehdr && phdr){
        for(int i = 0;i <ehdr->e_phnum; i++){
            if(phdr[i].p_type == PT_LOAD && seg_addrs[i]){
                if(munmap(seg_addrs[i], seg_sizes[i])!= 0){
                    perror("munmap failed");
                }
            }
        }
    }

    if(ehdr){
        free(ehdr);
        ehdr = NULL;
    }
    if(phdr){
        free(phdr);
        phdr = NULL;
    }
    
    if(fd >= 0){
        close(fd);
    }
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char** argv){
  fd = open(argv[1], O_RDONLY);

  if(fd < 0){
        perror("failed to open file");
        exit(1);
    }

  ehdr= malloc(sizeof(Elf32_Ehdr)); 

  if(!ehdr){ 
    perror("malloc ehdr error"); 
    exit(1); 
  }   

  if(read(fd, ehdr, sizeof(Elf32_Ehdr))!=sizeof(Elf32_Ehdr)){ 
        perror("ehdr is faulty");
        exit(1);
    }

// we check that we have the right file format using magic number(at start of ELF file, signify file format)
  if(ehdr->e_ident[0]!=0x7f || ehdr->e_ident[1]!='E' || ehdr->e_ident[2]!='L' ||ehdr->e_ident[3]!='F'){
        fprintf(stderr,"not a valid elf!!!!\n");
        exit(1);
    }

  off_t e_phoff =ehdr->e_phoff; 
  size_t e_phentsize =ehdr->e_phentsize; 
  uint16_t e_phnum =ehdr->e_phnum;

  if(lseek(fd,e_phoff,SEEK_SET) < 0){
        perror("lseek error for phoff");
        exit(1);
    }

  phdr= malloc(e_phnum*ehdr->e_phentsize);

  if(!phdr){
        perror("malloc failed for phdrs");
        exit(1);
    }

  if(read(fd, phdr, e_phnum*ehdr->e_phentsize)!= e_phnum*ehdr->e_phentsize){
          perror("failed to read phdr");
          exit(1);
    }

for (int i = 0; i < ehdr->e_phnum; i++){
    if(phdr[i].p_type != PT_LOAD) continue;


    //we map at a random virtual address as given in the mmap code in the assignment.
    void *virtual_mem= mmap(NULL, phdr[i].p_memsz,PROT_READ | PROT_WRITE | PROT_EXEC,MAP_ANONYMOUS | MAP_PRIVATE,0, 0);

    if(virtual_mem== MAP_FAILED){
        perror("mmap failed");
        exit(1);
    }

    if(lseek(fd, phdr[i].p_offset,SEEK_SET)< 0){
        perror("lseek failed");
        exit(1);
    }

    if(read(fd, virtual_mem, phdr[i].p_memsz)< 0){
        perror("read failed");
        exit(1);
    }

    //we store the segment counts and addresses for unloading later
    seg_addrs[i]= virtual_mem;
    seg_sizes[i]= phdr[i].p_memsz;
    seg_count++;
}

   
    Elf32_Addr entry=ehdr->e_entry;
    void *entry_addr= NULL;
    int entry_found= 0;
    
    //here we calculate the address of entry point, since we mapped at any random virtual address, we need to use rebasing to find address of _start
    //we find the offset of entry point from p_vaddr, add it to our seg_addrs[i](actaul address where segments mapped), and get the entry point address
    for(int i = 0;i <ehdr->e_phnum;i++) {
        if(phdr[i].p_type != PT_LOAD) continue;
        Elf32_Addr seg_start= phdr[i].p_vaddr;
        Elf32_Addr seg_end= phdr[i].p_vaddr + phdr[i].p_memsz;

        if(entry >= seg_start&& entry < seg_end){
            uintptr_t offset= (uintptr_t)entry -(uintptr_t)seg_start;
            entry_addr= (void *)((char *)seg_addrs[i] + offset);
            entry_found= 1;
            break;
        }
    }

    if(!entry_found){
        fprintf(stderr,"entrypoint not inside any pt_load\n");
        exit(1);
    }

    int (*start)()= (int(*)())entry_addr;

    int result= start();
    printf("User _start return value = %d\n",result);
}
