#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>

typedef char byte;

// Define cache states
#define INVALID 0
#define SHARED 1
#define EXCLUSIVE 2
#define MODIFIED 3

struct Cache {
    byte addr; // Address in memory
    byte data; // Data stored in cache
    byte state; // State for MESI protocol
};

struct Instruction {
    int type; // 0 for read, 1 for write
    byte addr;
    byte data; // Only used for write 
};

typedef struct Cache Cache;
typedef struct Instruction Instruction;

byte * memory;

Instruction decode_instruction_line(char * buffer){
    Instruction inst;
    char inst_type[3];
    sscanf(buffer, "%s", inst_type);
    if(!strcmp(inst_type, "RD")){
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.data = -1;
        inst.addr = addr;
    } else if(!strcmp(inst_type, "WR")){
        inst.type = 1;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.addr = addr;
        inst.data = val;
    }
    return inst;
}

void print_cache_lines(Cache * c, int cache_size){
    for(int i = 0; i < cache_size; i++){
        Cache cache_line = *(c+i);
        printf("Address: %d, State: %d, Data: %d\n", cache_line.addr, cache_line.state, cache_line.data);
    }
}

void cpu_loop(int thread_num, int num_threads){
    int cache_size = 2;
    Cache * cache = (Cache *) malloc(sizeof(Cache) * cache_size);
    if (cache == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }
    
    char filename[20];
    sprintf(filename, "input_%d.txt", thread_num);
    FILE * inst_file = fopen(filename, "r");
    if (inst_file == NULL) {
        printf("Failed to open file: %s\n", filename);
        free(cache);
        return;
    }
    
    char inst_line[20];
    while (fgets(inst_line, sizeof(inst_line), inst_file)){
        Instruction inst = decode_instruction_line(inst_line);
        
        int hash = inst.addr % cache_size;
        Cache cache_line = *(cache + hash);
        
        if(cache_line.addr != inst.addr){
            #pragma omp critical
            {
                if (cache_line.state == MODIFIED) {
                    *(memory + cache_line.addr) = cache_line.data;
                }
                cache_line.addr = inst.addr;
                cache_line.data = *(memory + inst.addr);
                cache_line.state = SHARED; 
            }
        }
        
        // Cache hit
        switch(inst.type){
            case 0: // Read
                printf("Thread %d: RD %d: %d\n", thread_num, cache_line.addr, cache_line.data);
                break;
            
            case 1: // Write
                cache_line.data = inst.data;
                cache_line.state = MODIFIED; 
                printf("Thread %d: WR %d: %d\n", thread_num, cache_line.addr, cache_line.data);
                break;
        }
        
        *(cache + hash) = cache_line;
    }
    fclose(inst_file);
    free(cache);
}

int main(int argc, char * argv[]){
    int mem_size = 24;
    memory = (byte *) malloc(sizeof(byte) * mem_size);
    int num_threads = 2; // Number of Input Files
    
    #pragma omp parallel num_threads(num_threads)
    {
        int thread_num = omp_get_thread_num();
        cpu_loop(thread_num, num_threads);
    }
    
    free(memory);
    return 0;
}
