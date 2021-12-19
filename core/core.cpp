/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <sys/stat.h>
#include <cmath>
using namespace std;
/* unix */
#include <unistd.h>

#define MAX64BIT UINT64_MAX
#define R_COUNT 16   /* registers */
#define CMP_COUNT 6 /* comparison types */
#define DECODE_COUNT 6  /* bits to decode in instruction */

/* BCC */
enum
{
    BCC_B = 0x8,    /* Unconditional branch */
    BCC_BEQ = 0x9,  /* Branch if equal  */
    BCC_BNE = 0xa,  /* Branch if not equal */
    BCC_BLE = 0xb,  /* Branch if lower or equal */
    BCC_BGE = 0xc,  /* Branch if greater or equal */
    BCC_BL = 0xd,   /* Branch if lower */
    BCC_BG = 0xe,   /* Branch if greater */
};

/* Opcodes */
enum
{
    OP_AND = 0x0,   /* Logical AND */
    OP_ORR = 0x1,   /* Logical OR  */
    OP_EOR = 0x2,   /* Logical XOR */
    OP_ADD = 0x3,   /* Addition */
    OP_ADC = 0x4,   /* Addition with carry */
    OP_CMP = 0x5,   /* Comparison */
    OP_SUB = 0x6,   /* Subtraction */
    OP_SBC = 0x7,   /* Subtraction with carry */
    OP_MOV = 0x8,   /* Move data */
    OP_LSH = 0x9,   /* Logical left shift */
    OP_RSH = 0xa,   /* Logical right shift */
};

uint32_t endian_swap(uint32_t instruction)
{
    instruction = (instruction>>24) | 
        ((instruction<<8) & 0x00FF0000) |
        ((instruction>>8) & 0x0000FF00) |
        (instruction<<24);
    return instruction;
}

uint32_t bitsextractor(uint32_t bits, int begin, int end){

    uint32_t mask = (1 <<(end - begin+1)) -1;
    return (bits >> begin) & mask;
}

int fetch(int PC,uint32_t instruction, bool cmpflags[], bool verbose){
    /* parse the instruction, and get the corresponding bits */
    uint32_t bcc = bitsextractor(instruction,28,31);
    uint32_t offset = bitsextractor(instruction,0,26);
    uint32_t sign = (instruction & ( 1 << 27 )) >> 27;
    PC++;
    switch (bcc)    /* switch case of bcc, and look on the cmpflags if the answer is true or false, if true we add offset*/
    {
        case BCC_B:
            PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BEQ:
            if (cmpflags[0]) PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BNE:
            if (cmpflags[1]) PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BLE:
            if (cmpflags[2]) PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BGE:
            if (cmpflags[3]) PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BL:
            if (cmpflags[4]) PC = PC + pow((-1),sign)*offset;
            break;
        case BCC_BG:
            if (cmpflags[5]) PC = PC + pow((-1),sign)*offset;
            break;   
        default:
            break;
    }
    if (verbose){
        printf("instruction = %x , PC = %u ",instruction,PC-1);
        if (bcc) printf("BCC = %x , offset = %x ",bcc,offset);
        printf("\n");
    }
    return PC; 
}

uint32_t* decode(uint32_t instruction, bool verbose){
    /* parse the instruction, and get the corresponding bits */
    uint32_t flag = (instruction & ( 1 << 24 )) >> 24;
    uint32_t op = bitsextractor(instruction,20,23);
    uint32_t ope1 = bitsextractor(instruction,16,19);
    uint32_t ope2 = bitsextractor(instruction,12,15);
    uint32_t dest = bitsextractor(instruction,8,11);
    uint32_t IV = bitsextractor(instruction,0,7);
    uint32_t *result = new uint32_t[DECODE_COUNT];
    result[0]= flag;
    result[1]= op;
    result[2]= ope1;
    result[3]= ope2;
    result[4]= dest;
    result[5]= IV;
    if (verbose) printf("opcode = %x , ope1 = %x , ope2 = %x , destination = %x , IV = %x , flag = %x \n",op,ope1,ope2,dest,IV,flag);
    return result;
}

void execute(uint64_t reg[], uint32_t flag, uint32_t op, uint32_t ope1, uint32_t ope2, uint32_t dest, uint32_t IV, uint32_t *carry, bool cmpflags[],bool verbose){
    memset(cmpflags,0,CMP_COUNT);   /* reset the values of the flags*/
    uint64_t value;
    if (flag) value = IV;
    else value = reg[ope2];
    switch (op) /* switch case of op code, and execute different operations corresponding */
    {
        case OP_AND:
            if (verbose) printf("AND OPERATION \n");
            reg[dest] = reg[ope1] & value;
            break;
        case OP_ORR:
            if (verbose) printf("ORR OPERATION \n");
            reg[dest] = reg[ope1] | value;
            break;
        case OP_EOR:
            if (verbose) printf("EOR OPERATION \n");
            reg[dest] = reg[ope1] ^ value;
            break;
        case OP_ADD:
            if (verbose) printf("ADD OPERATION \n");
            if ((value > 0) && (reg[ope1] > MAX64BIT - value)) *carry=1;
            reg[dest] = reg[ope1] + value;
            break;
        case OP_ADC:
            if (verbose) printf("ADC OPERATION \n");
            reg[dest] = reg[ope1] + value + *carry;
            if (reg[ope1] > MAX64BIT - value - *carry) *carry=1; 
            else *carry = 0;
            break;
        case OP_CMP:
            if (verbose) printf("CMP OPERATION \n");
            if( reg[ope1] == value) cmpflags[0]=1;
            if( reg[ope1] != value) cmpflags[1]=1;
            if( reg[ope1] <= value) cmpflags[2]=1;
            if( reg[ope1] >= value) cmpflags[3]=1;
            if( reg[ope1] < value) cmpflags[4]=1;
            if( reg[ope1] > value) cmpflags[5]=1;
            break;
        case OP_SUB:
            if (verbose) printf("SUB OPERATION \n");
            if ((value > 0) && (reg[ope1] < value )) *carry=1;
            reg[dest] = reg[ope1] - value;
            break;
        case OP_SBC:
            if (verbose) printf("SBC OPERATION \n");
            reg[dest] = reg[ope1] - value + *carry -1;
            if (reg[ope1] < value - *carry +1) *carry=1; 
            else *carry = 0;
            break;
        case OP_MOV:
            if (verbose) printf("MOV OPERATION \n");
            reg[dest] = value;
            break;
        case OP_LSH:
            if (verbose) printf("LSH OPERATION \n");
            reg[dest] = reg[ope1] << value;
            break;
        case OP_RSH:
            if (verbose) printf("RSH OPERATION \n");
            reg[dest] = reg[ope1] >> value;
            break;
        default:
            break;
    }
    if (verbose)
    {
        printf("carry = %x , BEQ = %x , BNE = %x , BLE = %x , BGE = %x , BL = %x , BG = %x \n",*carry,cmpflags[0],cmpflags[1],cmpflags[2],cmpflags[3],cmpflags[4],cmpflags[5]);
        for (int i = 0; i < R_COUNT; i++) printf("R%d=%08lx ",i,reg[i]);
        printf("\n");
    }
}

int concat(int a, int b)    /* concatenate two ints */
{
    int result = stoi(to_string(a) + to_string(b));
    return result;
}

void readinitstate(const char* initstate,uint64_t reg[]){
    FILE *ptr;
    ptr = fopen(initstate,"rb");
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char *cutline;
    while ( (read = getline(&line, &len, ptr)) != -1 ) {
        if ( ( (line[0]=='R') | (line[0]=='r') ) & ((line[2]=='=') | (line[3]=='=')) )
        {
            int regnumber;
            switch (line[2])
            {
            case '=': regnumber = line[1] - '0'; /* convert char to int */
                break;
            default: regnumber = concat(line[1] - '0' , line[2] - '0');
                break;
            }
            if (regnumber < R_COUNT){ /* parsing the file */
                char copyline[len];
                strcpy(copyline, line);
                cutline = strtok(copyline,"="); 
                cutline = strtok(NULL,"\n"); 
                reg[regnumber] = (uint64_t)strtoul(cutline, NULL, 16);
            }
        }
    }
    fclose(ptr);
    free(line);
}

off_t getprogramsize(const char* inittest){
    struct stat st;
    stat(inittest, &st); 
    off_t size = st.st_size;
    size = size/4;
    return size;
}

void launch(const char* inittest,const char* initstate, bool verbose){
    int PC = 0; /* program counter */
    uint64_t reg[R_COUNT] = {0};    /* array of registers */
    bool cmpflags[CMP_COUNT];   /* array that will be used to store CMP comparison */
    uint32_t carry = 0; /* initializing carry */
    /* read inittest */
    off_t size = getprogramsize(inittest);  /* get the size of the inittest file */
    uint32_t program[size]; /* initializing the array of instructions (32 bits) */
    FILE *ptr;
    ptr = fopen(inittest,"rb"); /* read inittest */
    fread(program,sizeof(program),1,ptr); 
    for (int i = 0; i < size; i++) program[i] = endian_swap(program[i]);    /* switch from little to big endian */
    fclose(ptr);
    /* read initstate */
    readinitstate(initstate,reg);
    /* begin core execution */
    uint32_t *decoded_instruction = new uint32_t[DECODE_COUNT]; /* used to store the results of decode, such as flag, bcc, op ... */
    if (verbose) printf("BEGIN PARSING\n-----------------------------------------------------------------------------------------------\n");
    while (PC < size)
    {
        /* launch the core, in the order : fetch, decode, execute */
        PC = fetch(PC,program[PC],cmpflags,verbose);
        decoded_instruction = decode(program[PC-1],verbose);
        execute(reg,decoded_instruction[0],decoded_instruction[1],decoded_instruction[2],decoded_instruction[3],decoded_instruction[4],decoded_instruction[5],&carry,cmpflags,verbose);
        if (verbose) printf("-----------------------------------------------------------------------------------------------\n");
    }
    if (verbose) printf("END PARSING\n");
    /* we print the final registers */
    printf("Registers final states : \n");
    for (int i = 0; i < R_COUNT; i++) printf("R%d=%08lx ",i,reg[i]);
    printf("\n");
}

int in_array(const char *array[], int size,const char *lookfor)
{
    for (int i = 0; i < size; i++)
        if (strcmp(lookfor, array[i]) == 0) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int main(int argc, const char* argv[])
{
    /* Handle the user interactions when they start the program */
    if (argc==1) printf("Invalid usage of \"./core\". Try -h for help.\n");
    else if ( in_array(argv,argc,"-h") ) system("man ./coreman");
    else if (argc==4){
        if (strcmp(argv[3],"-v")==0) {
            /* Check if files exists */
            if ((access(argv[1],F_OK) == 0) && (access(argv[2],F_OK) == 0)){
                printf("Starting C program with verbose mode\n");
                launch(argv[1],argv[2],1);
            } else printf("One of the files does not exist.\nCode file: %s \nState file: %s \n",(access(argv[1],F_OK) == 0) ? "Exists" : "Doesn't exists", (access(argv[2],F_OK) == 0) ? "Exists" : "Doesn't exists" );
        } else printf("Invalid usage of \"./core\". Try -h for help.\n");
    } else if (argc==3){
        /* Check if files exists */
        if ((access(argv[1],F_OK) == 0) && (access(argv[2],F_OK) == 0)){
            printf("Starting C program\n");
            launch(argv[1],argv[2],0);
        } else printf("One of the files does not exist.\nCode file: %s \nState file: %s \n",(access(argv[1],F_OK) == 0) ? "Exists" : "Doesn't exists", (access(argv[2],F_OK) == 0) ? "Exists" : "Doesn't exists" );
    } else printf("Invalid usage of \"./core\". Try -h for help.\n");
    return EXIT_SUCCESS;
}