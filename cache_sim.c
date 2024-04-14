#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>

typedef char byte;

#define CACHE_SIZE 2
#define MEMORY_SIZE 24

// MESI Protocol states
#define INVALID 0
#define SHARED 1
#define EXCLUSIVE 2
#define MODIFIED 3

struct cache {
    byte address; // This is the address in memory.
    byte value; // This is the value stored in cached memory.
    byte state; // MESI protocol state
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    byte address;
    byte value; // Only used for WR
};

typedef struct cache cache;
typedef struct decoded_inst decoded;

byte * memory;

// Decode instruction lines
decoded decode_inst_line(char * buffer){
    decoded inst;
    char inst_type[3]; // Increased size to accommodate "WR" and "RD"
    sscanf(buffer, "%s", inst_type);
    if(!strcmp(inst_type, "RD")){
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    } else if(!strcmp(inst_type, "WR")){
        inst.type = 1;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;
    }
    return inst;
}

// Helper function to print the cachelines
void print_cachelines(cache * c, int cache_size){
    for(int i = 0; i < cache_size; i++){
        cache cacheline = *(c+i);
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}

// Coherence check function based on MESI protocol
void coherence_check(cache *c, int hash, decoded inst) {
    cache cacheline = *(c+hash);
    switch(cacheline.state) {
        case INVALID:
            cacheline.state = EXCLUSIVE;
            break;
        case SHARED:
            cacheline.state = INVALID;
            break;
        case EXCLUSIVE:
            break;
        case MODIFIED:
            *(memory + cacheline.address) = cacheline.value;
            cacheline.state = EXCLUSIVE;
            break;
    }
    *(c+hash) = cacheline;
}

// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int thread_id){
    // Initialize a CPU level cache
    cache * c = (cache *) malloc(sizeof(cache) * CACHE_SIZE);
    memset(c, 0, sizeof(cache) * CACHE_SIZE); // Initialize cache to zero
    
    // Read Input file
    char file_name[20];
    sprintf(file_name, "input_%d.txt", thread_id);
    printf("Thread %d: File name: %s\n", thread_id, file_name); // Debug print
    FILE * inst_file = fopen(file_name, "r");
    if (inst_file == NULL) {
        printf("Thread %d: Error opening file %s\n", thread_id, file_name); // Debug print
        return;
    }
    char inst_line[20];
    // Decode instructions and execute them.
    while (fgets(inst_line, sizeof(inst_line), inst_file)){
        decoded inst = decode_inst_line(inst_line);
        
        // Cache Replacement Algorithm
        int hash = inst.address % CACHE_SIZE;
        
        // Coherence check based on MESI protocol
        coherence_check(c, hash, inst);
        #pragma omp barrier // Wait for all threads to complete coherence check
        
        cache cacheline = *(c+hash);
        
        // Fetch data from cache or memory
        if(cacheline.address != inst.address){
            // Fetch data from memory
            cacheline.address = inst.address;
            cacheline.state = EXCLUSIVE; // New data is always EXCLUSIVE
            cacheline.value = *(memory + inst.address);
            if(inst.type == 1){
                cacheline.value = inst.value;
            }
            *(c+hash) = cacheline;
        }
        
        // Print operation
        #pragma omp barrier // Synchronize before printing
        switch(inst.type){
            case 0:
                printf("Thread %d: RD %d: %d\n", thread_id, cacheline.address, cacheline.value);
                break;
            
            case 1:
                printf("Thread %d: WR %d: %d\n", thread_id, cacheline.address, cacheline.value);
                break;
        }
        #pragma omp barrier // Synchronize after printing
    }
    free(c);
}

int main(int argc, char * argv[]){
    // Initialize Global memory
    memory = (byte *) malloc(sizeof(byte) * MEMORY_SIZE);
    
    // Execute CPU loop in parallel using OpenMP
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        cpu_loop(thread_id);
    }
    
    free(memory);
}
