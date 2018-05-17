/***************************************************************************
 * *    Inf2C-CS Coursework 2: TLB and Cache Simulation
 * *
 * *    Instructor: Boris Grot
 * *
 * *    TA: Priyank Faldu
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
/* Do not add any more header files */

/*
 * Various structures
 */
typedef enum {tlb_only, cache_only, tlb_cache} hierarchy_t;
typedef enum {instruction, data} access_t;
const char* get_hierarchy_type(uint32_t t) {
    switch(t) {
        case tlb_only: return "tlb_only";
        case cache_only: return "cache-only";
        case tlb_cache: return "tlb+cache";
        default: assert(0); return "";
    };
    return "";
}

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

// These are statistics for the cache and TLB and should be maintained by you.
typedef struct {
    uint32_t tlb_data_hits;
    uint32_t tlb_data_misses;
    uint32_t tlb_instruction_hits;
    uint32_t tlb_instruction_misses;
    uint32_t cache_data_hits;
    uint32_t cache_data_misses;
    uint32_t cache_instruction_hits;
    uint32_t cache_instruction_misses;
} result_t;


/*
 * Parameters for TLB and cache that will be populated by the provided code skeleton.
 */
hierarchy_t hierarchy_type = tlb_cache;
uint32_t number_of_tlb_entries = 0;
uint32_t page_size = 0;
uint32_t number_of_cache_blocks = 0;
uint32_t cache_block_size = 0;
uint32_t num_page_table_accesses = 0;


/*
 * Each of the variables (subject to hierarchy_type) below must be populated by you.
 */
uint32_t g_total_num_virtual_pages = 0;
uint32_t g_num_tlb_tag_bits = 0;
uint32_t g_tlb_offset_bits = 0;
uint32_t g_num_cache_tag_bits = 0;
uint32_t g_cache_offset_bits= 0;
result_t g_result;


/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access)
 * 2) 32-bit virtual memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1002];
    char* token = NULL;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file)!=NULL) {

        /* Get the access type */
        token = strsep(&string, " \n");
        if (strcmp(token,"I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token,"D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(-1);
        }

        /* Get the address */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);

        return access;
    }

    /* If there are no more entries in the file return an address 0 */
    access.address = 0;
    return access;
}

/*
 * Call this function to get the physical page number for a given virtual number.
 * Note that this function takes virtual page number as an argument and not the whole virtual address.
 * Also note that this is just a dummy function for mimicing translation. Real systems maintains multi-level page tables.
 */
uint32_t dummy_translate_virtual_page_num(uint32_t virtual_page_num) {
    uint32_t physical_page_num = virtual_page_num ^ 0xFFFFFFFF;
    num_page_table_accesses++;
    if ( page_size == 256 ) {
        physical_page_num = physical_page_num & 0x00FFF0FF;
    } else {
        assert(page_size == 4096);
        physical_page_num = physical_page_num & 0x000FFF0F;
    }
    return physical_page_num;
}

void print_statistics(uint32_t num_virtual_pages, uint32_t num_tlb_tag_bits, uint32_t tlb_offset_bits, uint32_t num_cache_tag_bits, uint32_t cache_offset_bits, result_t* r) {
    /* Do Not Modify This Function */

    printf("NumPageTableAccesses:%u\n", num_page_table_accesses);
    printf("TotalVirtualPages:%u\n", num_virtual_pages);
    if ( hierarchy_type != cache_only ) {
        printf("TLBTagBits:%u\n", num_tlb_tag_bits);
        printf("TLBOffsetBits:%u\n", tlb_offset_bits);
        uint32_t tlb_total_hits = r->tlb_data_hits + r->tlb_instruction_hits;
        uint32_t tlb_total_misses = r->tlb_data_misses + r->tlb_instruction_misses;
        printf("TLB:Accesses:%u\n", tlb_total_hits + tlb_total_misses);
        printf("TLB:data-hits:%u, data-misses:%u, inst-hits:%u, inst-misses:%u\n", r->tlb_data_hits, r->tlb_data_misses, r->tlb_instruction_hits, r->tlb_instruction_misses);
        printf("TLB:total-hit-rate:%2.2f%%\n", tlb_total_hits / (float)(tlb_total_hits + tlb_total_misses) * 100.0);
    }

    if ( hierarchy_type != tlb_only ) {
        printf("CacheTagBits:%u\n", num_cache_tag_bits);
        printf("CacheOffsetBits:%u\n", cache_offset_bits);
        uint32_t cache_total_hits = r->cache_data_hits + r->cache_instruction_hits;
        uint32_t cache_total_misses = r->cache_data_misses + r->cache_instruction_misses;
        printf("Cache:data-hits:%u, data-misses:%u, inst-hits:%u, inst-misses:%u\n", r->cache_data_hits, r->cache_data_misses, r->cache_instruction_hits, r->cache_instruction_misses);
        printf("Cache:total-hit-rate:%2.2f%%\n", cache_total_hits / (float)(cache_total_hits + cache_total_misses) * 100.0);
    }
}


 //TLB
//these two arrays must always be kept in sync!
uint32_t *qtlb; //tlb virtual tag storage
uint32_t *tlbtable; //tlb physical tag storage

void removelrutlb(){ //this function removes the last element o both arrays, and shifts all other elemnts to index+1
  int i = number_of_tlb_entries-2;
  while(i >= 0){
    qtlb[i+1] = qtlb[i];
    tlbtable[i+1] = tlbtable[i];
    i = i - 1;
  }
}

void movetofrontoflru(int index){ //this function moves elements at input index to index 0, and causes entries lower
  uint32_t qstorev = qtlb[index]; //in the array oder to be shifted right one
  uint32_t qstorep = tlbtable[index];
  int i = index;
  while(i > 0){
    qtlb[i] = qtlb[i-1];
    tlbtable[i] = tlbtable[i-1];
    i = i - 1;
  }
  qtlb[0] = qstorev;
  tlbtable[0] = qstorep;
}
//CACHE

uint32_t *cache;


int main(int argc, char** argv) {

    /*
     *
     * Read command-line parameters and initialize configuration variables.
     *
     */
    int improper_args = 0;
    char file[10000];
    if ( argc < 2 ) {
        improper_args = 1;
        printf("Usage: ./mem_sim [hierarchy_type: tlb-only cache-only tlb+cache] [number_of_tlb_entries: 8/16] [page_size: 256/4096] [number_of_cache_blocks: 256/2048] [cache_block_size: 32/64] mem_trace.txt\n");
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */
        if ( strcmp(argv[1], "tlb-only") == 0 ) {
            if ( argc != 5 ) {
                improper_args = 1;
                printf("Usage: ./mem_sim tlb-only [number_of_tlb_entries: 8/16] [page_size: 256/4096] mem_trace.txt\n");
            } else {
                hierarchy_type = tlb_only;
                number_of_tlb_entries = atoi(argv[2]);
                page_size = atoi(argv[3]);
                strcpy(file, argv[4]);
            }
        } else if ( strcmp(argv[1], "cache-only") == 0 ) {
            if ( argc != 6 ) {
                improper_args = 1;
                printf("Usage: ./mem_sim cache-only [page_size: 256/4096] [number_of_cache_blocks: 256/2048] [cache_block_size: 32/64] mem_trace.txt\n");
            } else {
                hierarchy_type = cache_only;
                page_size = atoi(argv[2]);
                number_of_cache_blocks = atoi(argv[3]);
                cache_block_size = atoi(argv[4]);
                strcpy(file, argv[5]);
            }
        } else if ( strcmp(argv[1], "tlb+cache") == 0 ) {
            if ( argc != 7 ) {
                improper_args = 1;
                printf("Usage: ./mem_sim tlb+cache [number_of_tlb_entries: 8/16] [page_size: 256/4096] [number_of_cache_blocks: 256/2048] [cache_block_size: 32/64] mem_trace.txt\n");
            } else {
                hierarchy_type = tlb_cache;
                number_of_tlb_entries = atoi(argv[2]);
                page_size = atoi(argv[3]);
                number_of_cache_blocks = atoi(argv[4]);
                cache_block_size = atoi(argv[5]);
                strcpy(file, argv[6]);
            }
        } else {
            printf("Unsupported hierarchy type: %s\n", argv[1]);
            improper_args = 1;
        }
    }
    if ( improper_args ) {
        exit(-1);
    }
    assert(page_size == 256 || page_size == 4096);
    if ( hierarchy_type != cache_only) {
        assert(number_of_tlb_entries == 8 || number_of_tlb_entries == 16);
    }
    if ( hierarchy_type != tlb_only) {
        assert(number_of_cache_blocks == 256 || number_of_cache_blocks == 2048);
        assert(cache_block_size == 32 || cache_block_size == 64);
    }

    printf("input:trace_file: %s\n", file);
    printf("input:hierarchy_type: %s\n", get_hierarchy_type(hierarchy_type));
    printf("input:number_of_tlb_entries: %u\n", number_of_tlb_entries);
    printf("input:page_size: %u\n", page_size);
    printf("input:number_of_cache_blocks: %u\n", number_of_cache_blocks);
    printf("input:cache_block_size: %u\n", cache_block_size);
    printf("\n");

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file =fopen(file,"r");
    if (!ptr_file) {
        printf("Unable to open the trace file: %s\n", file);
        exit(-1);
    }

    /* result structure is initialized for you. */
    memset(&g_result, 0, sizeof(result_t));

    /* Do not delete any of the lines below.
     * Use the following snippet and add your code to finish the task. */


    mem_access_t access;
    if(hierarchy_type == tlb_only){ //check that we are in tlb_only mode
      int toShift;
      if(page_size == 4096){ //if page size is 4096, set up system accordingly
        toShift = 12; //offset for tlb
        g_total_num_virtual_pages = 1048576;
      }
      if(page_size == 256){//same for 256
        toShift = 8;
        g_total_num_virtual_pages = 16777216;
      }
      g_tlb_offset_bits = toShift; //set the gobal offset variable
      g_num_tlb_tag_bits = 32 - toShift; //set the global number of tlb tag bits
      qtlb = (uint32_t*) calloc(number_of_tlb_entries, 32 - toShift); //initialise the tlb virtual tag storage
      tlbtable = (uint32_t*) calloc(number_of_tlb_entries, 32 - toShift); //do same for physical
      /* Loop until the whole trace file has been read. */
      while(1) {
          access = read_transaction(ptr_file);
        // If no transactions left, break out of loop.
          if (access.address == 0){
              break;
          }
          uint32_t aAdd = access.address; //get the 32bit address from access
          aAdd = aAdd >> toShift; //shift it right by toShift to obtain virtual tag
          int i = 0;
          int x = 0; //use this as a boolean
          int y;
          while(i < number_of_tlb_entries){ //iterate over all the tlb virtual tags, if we have a match, we dont need to replace anything, just change the lru order
            if(qtlb[i] == aAdd){
              x = 1;
              y = i;
            }
            i++;
          }
          if(x == 1){ //this is the section that follows a tlb hit
            if(access.accesstype == instruction){ //determine if it was an instruction or data hit
              g_result.tlb_instruction_hits = g_result.tlb_instruction_hits + 1; //increment instruction hit variable
              movetofrontoflru(y); //then move the virtual and physical tags to the front of the lru ordering (as they are now the most recently used)
            }else{
              g_result.tlb_data_hits = g_result.tlb_data_hits + 1;//the same but for a data hit
              movetofrontoflru(y);
            }
          }
          if(x == 0){ //this is the section following a tlb miss
            if(access.accesstype == instruction){ //determine whether we have an instruction or data miss
              g_result.tlb_instruction_misses = g_result.tlb_instruction_misses + 1; //increment the counter for this miss
              removelrutlb(); //this function removes the least recently used entry fom our tlb, and then moves all other entries one along the order
              qtlb[0] = aAdd; //we then add the virtual tag to the tlb
              uint32_t qc = dummy_translate_virtual_page_num(access.address); //to obtain the physical tag, we first need the physical address
              qc >> toShift; //shift this right to obtain physical tag
              tlbtable[0] = qc; //then add this new tag to the tlb physical tag storage
            }else{
              g_result.tlb_data_misses = g_result.tlb_data_misses + 1; //same as above, but for data miss
              removelrutlb();
              qtlb[0] = aAdd;
              uint32_t qc = dummy_translate_virtual_page_num(access.address);
              qc >> toShift;
              tlbtable[0] = qc;
            }
          }
        }
      }
      if(hierarchy_type == cache_only){ //cache_only mode
        int toShift;
        if(page_size == 4096){ //if page size is 4096, set up system accordingly
          g_total_num_virtual_pages = 1048576;
        }
        if(page_size == 256){//same for 256
          g_total_num_virtual_pages = 16777216;
        }
        if(cache_block_size == 32){
          toShift = 5;
        }
        if(cache_block_size == 64){
          toShift = 6;
        }
        int x = 1;
        int cacheindex = 0;
        while (x <= numBlocks){
            x = x*2;
            cacheindex++;
        }
        toShift += cacheindex;
        cache = (uint32_t*) calloc(number_of_cache_blocks, 32-toShift);
        while(1) {
            access = read_transaction(ptr_file);
          // If no transactions left, break out of loop.
            if (access.address == 0){
                break;
            }
            uint32_t va = access.address;
            uint32_t pa = dummy_translate_virtual_page_num(va);
            pa >> toShift;

      }

    /* Do not modify code below. */
    /* Make sure that all the parameters are appropriately populated. */
    print_statistics(g_total_num_virtual_pages, g_num_tlb_tag_bits, g_tlb_offset_bits, g_num_cache_tag_bits, g_cache_offset_bits, &g_result);

    /* Close the trace file. */
    fclose(ptr_file);
    return 0;
}
