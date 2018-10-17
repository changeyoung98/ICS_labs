/*
 *
 * Student Name:Chen Zhiyang
 * Student ID:516030910347
 *
 */
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>

typedef struct{
    int valid;       
    int tag;        
    int used_time;   
} Line;

typedef struct{
    Line* lines;
    int line_num;   //E    
} Set;

typedef struct {
    int set_num;    //S   
    Set* sets;      
} Cache;

#define MAX_FILENAME 255
bool verbose = false;
int s, b, t, S, B, E;
int hit = 0, miss = 0, eviction = 0;
 Cache* cache;

#define GET_TAG(x) (((x) >> (s + b)) & ((1L << t) - 1))
#define GET_SET(x) (((x) >> b) & ((1 << s) - 1))
#define GET_OFFSET(x)((x)&((1<<b)-1))
#define M 64
void print_usage(){
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
/*parse command line*/
void parse_command(int argc, char **argv, char* filename){
    int option;
     memset(filename, 0, MAX_FILENAME + 1);
    while((option = getopt(argc,argv,"hvs:E:b:t:")) != -1){
        switch(option){
            case 'h':
                print_usage();
                break;
            case 'v':
                verbose = true;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E= atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                 strncpy(filename, optarg, MAX_FILENAME);
                break;
            default:
                print_usage();
                  break;
        } 
    }

if(s <= 0 || E <= 0 || b <= 0){
        print_usage();
  }
}

/*initialize cache*/
void Cache_init(int s,int E,int b)
{
    int i,j;
    int S = 1<<s;
    Set* set;
    set = (Set*)malloc(S*sizeof(Set));
    for(i = 0;i < S;i++) {
        set[i].lines = (Line*)malloc(sizeof(Line)*E);
        for(j = 0;j < E;j++) {
            set[i].lines[j].valid = 0;
            set[i].lines[j].tag = 0;
            set[i].lines[j].used_time = 0;
        }
    }
    cache->sets=set;
}


/* find the cache line related to the address*/
Line* find_line(int tag,int set_index, int block_offset){
    Line* lines = cache->sets[set_index].lines;
    for(int i = 0; i < E; i++){
        Line *line = &lines[i];
        if(line->valid && line->tag == tag){
            return line;
        }
    }
    return NULL; 
}


/* find the line that the data should be stored in */
Line *find_store(int tag, int set_index, bool *eviction_c){
    Line *lines = cache->sets[set_index].lines;
    Line *lru_line = &lines[0];
    
    for(int i = 0; i <E; i++){
        Line *line = &lines[i];
        if(!line->valid){
            *eviction_c = false;
            return line;
        }
        if(line->valid && line->used_time < lru_line->used_time){
            lru_line = line;
        }
    }

    *eviction_c = true;
    return lru_line;
}

/* update the state of the lines */
void update_cache(int tag,Line *line){
    line->valid = true;
    line->tag = tag;
    line->used_time = clock();
}

void print_detail(char access_type,long address,int byte_size,bool hit_c,bool eviction_c){
    if(!verbose){
        return;
    } 

    switch(access_type){
        case 'L':
        case 'S':
            if(hit_c){
                printf("%c %lx,%d hit\n",access_type,address,byte_size);
            }
            else if(eviction_c){
                printf("%c %lx,%d miss eviction\n",access_type,address,byte_size);
            }
            else{
                printf("%c %lx,%d miss\n",access_type,address,byte_size);
            }
            break;
        case 'M':
            if(hit_c){
                printf("%c %lx,%d hit hit\n",access_type,address,byte_size);
            }
            else if(eviction_c){
                printf("%c %lx,%d miss eviction hit\n",access_type,address,byte_size);
            }
            else{
                printf("%c %lx,%d miss hit\n",access_type,address,byte_size);
            }
            break;
        default:
            break;
    }
}

void deal_with_file(char* filename){
   char op;
   long addr;
   int byte_size;
   FILE *fin;

   if(!(fin = fopen(filename,"r"))){
        printf("No such file or diretory %s",filename);
    }
   while(fscanf(fin,"%c %lx,%d",&op,&addr,&byte_size) != EOF){
       Line *line = NULL;
        /* hit and eviction*/
        bool hit_c= true;
        bool eviction_c = false;

        /* Check for access type  */
        if(!(op == 'L' || op == 'M' || op == 'S')){
            continue;
        }

        int tag=GET_TAG(addr);
        int set_index=GET_SET(addr);
        int offset=GET_OFFSET(addr);
        /* Find cache line */
        if((line = find_line(tag,set_index,offset)) == NULL){
            hit_c = false;
            line = find_store(tag,set_index,&eviction_c);
        }
        print_detail(op,addr,byte_size,hit_c,eviction_c);

        /* General condition for 'S','L' and the load operation of 'M' */
        if(hit_c){
            hit++;
        }
        else{
            miss++;
        }
        if(eviction_c){
            eviction++;
        }

        /* The store operation of 'M' will hit certainly */
        if(op == 'M'){
            hit++;
        }
        update_cache(tag,line); 
    }
}

int main(int argc,char* argv[])
{
    char filename[MAX_FILENAME + 1];
    parse_command(argc,argv,filename);
    S=1<<s;
    B=1<<b;
    t=M-s-b;
    cache=(Cache*)malloc(sizeof(Cache)) ; 
    Cache_init(s,E,b);
    deal_with_file(filename);
    printSummary(hit, miss, eviction);
    return 0;
}
