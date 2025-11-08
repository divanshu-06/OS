#include "loader.h"

#define PAGE_SIZE 4096

//variables used by signal handeller
Elf32_Ehdr *ehdr=NULL;
Elf32_Phdr *phdrs=NULL;
int fd=-1;

typedef struct {
    Elf32_Phdr ph;
    size_t page_count;
    uint8_t *page_alloc;
} seg_info_t;

seg_info_t *segs=NULL;

//stats
volatile size_t page_fault_count=0;
volatile size_t page_alloc_count=0;
volatile size_t internal_frag_bytes=0;

//convert ELF pflags to prot
static int phflags_to_prot(uint32_t p_flags){
    int prot=0;
    if (p_flags & PF_R) {
        prot |= PROT_READ;
    }

    if (p_flags & PF_W){
         prot |= PROT_WRITE;
    }

    if (p_flags & PF_X){
         prot |= PROT_EXEC;
    }

    return prot;
}

//find segment index for a faulting address
static int find_segment_for_addr(uintptr_t addr) {
    for(int i=0;i<ehdr->e_phnum;++i) {
        if(phdrs[i].p_type!=PT_LOAD) continue;
        uintptr_t seg_start=(uintptr_t)phdrs[i].p_vaddr;
        uintptr_t seg_end=seg_start+(uintptr_t)phdrs[i].p_memsz;
        if (addr >= seg_start && addr<seg_end) return i;
    }
    return -1;
}

// SIGSEGV handler for lazy loading 
static void segv_handler(int sig,siginfo_t *si,void *unused){
    (void)sig;
    (void)unused;
    
    uintptr_t fault=(uintptr_t)si->si_addr;
    uintptr_t page_base=fault & ~(PAGE_SIZE-1);

    int seg_idx=find_segment_for_addr(fault);
    if (seg_idx<0) {
        /* Not in any PT_LOAD range-terminate */
        signal(SIGSEGV,SIG_DFL);
        raise(SIGSEGV);
        return;
    }

    seg_info_t *s=&segs[seg_idx];
    uintptr_t seg_vstart=(uintptr_t)s->ph.p_vaddr;
    size_t page_idx=(page_base-seg_vstart)/PAGE_SIZE;

    if (page_idx >= s->page_count||s->page_alloc[page_idx]) {
        //Out of bounds or already allocated so it terminates 
        signal(SIGSEGV,SIG_DFL);
        raise(SIGSEGV);
        return;
    }

    // Map one page with RW permissions 
    void *map_addr=mmap((void*)page_base,PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (map_addr==MAP_FAILED) {
        signal(SIGSEGV,SIG_DFL);
        raise(SIGSEGV);
        return;
    }

    //Calculate bytes to read from file 
    off_t page_file_off_in_seg=(off_t)page_idx*PAGE_SIZE;
    size_t bytes_from_file=0;
    if (page_file_off_in_seg<(off_t)s->ph.p_filesz) {
        off_t remaining=(off_t)s->ph.p_filesz-page_file_off_in_seg;
        bytes_from_file=(size_t)(remaining > PAGE_SIZE ? PAGE_SIZE : remaining);
    }

    //Read from file if needed 
    if (bytes_from_file > 0){
        off_t file_offset=(off_t)s->ph.p_offset +page_file_off_in_seg;
        if(pread(fd,map_addr,bytes_from_file,file_offset)!=(ssize_t)bytes_from_file){
            munmap(map_addr,PAGE_SIZE);
            signal(SIGSEGV,SIG_DFL);
            raise(SIGSEGV);
            return;
        }
    }

    //zero remainder of page (BSS)
    if(bytes_from_file<PAGE_SIZE){
        memset((char*)map_addr+bytes_from_file,0,PAGE_SIZE-bytes_from_file);
    }

    //set final protections
    mprotect((void*)page_base,PAGE_SIZE,phflags_to_prot(s->ph.p_flags));

    //Updating  stats 
    s->page_alloc[page_idx]=1;
    __sync_fetch_and_add(&page_fault_count,1);
    __sync_fetch_and_add(&page_alloc_count,1);
    
    uintptr_t seg_end = seg_vstart + s->ph.p_memsz;
    uintptr_t page_end = page_base + PAGE_SIZE;
    
    //check if this page extends beyond the segment boundary
    if (page_end > seg_end) {
        //this is the last page - calculate how much extends beyond
        size_t bytes_beyond = page_end - seg_end;
        __sync_fetch_and_add(&internal_frag_bytes, bytes_beyond);
    }
}
