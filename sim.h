#pragma once
#pragma once
#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Constants and parameters
#define NUM_CORES            4
#define MAX_IMEM_SIZE        1024
#define MAIN_MEM_SIZE        (1 << 20)   // up to 2^20 words
#define REGFILE_SIZE         16

// Data cache parameters: 512 words total, 8 words per block -> 64 lines direct-mapped.
#define DCACHE_NUM_LINES     64
#define DCACHE_BLOCK_SIZE    8
#define DCACHE_NUM_WORDS     (DCACHE_NUM_LINES * DCACHE_BLOCK_SIZE)

#define MAIN_MEMORY_DELAY    16

// 20-bit address: offset = 3 bits (8 words per line) , index  = 6 bits (64 lines) , tag    = 11 bits (20 - 3 - 6)
static inline uint32_t get_offset(uint32_t addr) { return (addr & 0x7u); }
static inline uint32_t get_indx(uint32_t addr) { return ((addr >> 3) & 0x3Fu); }
static inline uint32_t get_tag(uint32_t addr) { return ((addr >> 9) & 0x7FFu); }

// MESI states
#define MESI_INVALID   0
#define MESI_SHARED    1
#define MESI_EXCLUSIVE 2
#define MESI_MODIFIED  3

// Bus commands
#define BUS_NONE       0
#define BUS_RD         1  // BusRd
#define BUS_RDX        2  // BusRdX
#define BUS_FLUSH      3  // Flush 

// Instruction opcodes
#define OPCODE_ADD     0
#define OPCODE_SUB     1
#define OPCODE_AND     2
#define OPCODE_OR      3
#define OPCODE_XOR     4
#define OPCODE_MUL     5
#define OPCODE_SLL     6
#define OPCODE_SRA     7
#define OPCODE_SRL     8
#define OPCODE_BEQ     9
#define OPCODE_BNE     10
#define OPCODE_BLT     11
#define OPCODE_BGT     12
#define OPCODE_BLE     13
#define OPCODE_BGE     14
#define OPCODE_JAL     15
#define OPCODE_LW      16
#define OPCODE_SW      17
#define OPCODE_HALT    20


 // Instruction format

 // Bits 31:24: opcode
 // Bits 23:20: rd
 // Bits 19:16: rs
 // Bits 15:12: rt
 // Bits 11:0 : imm

typedef union {
    uint32_t raw;
    struct {
        unsigned int imm : 12;
        unsigned int rt : 4;
        unsigned int rs : 4;
        unsigned int rd : 4;
        unsigned int opcode : 8;
    } fields;
} Instruction;

// Data cache structures
typedef struct {
    uint8_t  mesi;
    uint16_t tag;
    uint32_t block[DCACHE_BLOCK_SIZE];
} CacheLine;

typedef struct {
    CacheLine lines[DCACHE_NUM_LINES];
} Cache;

// Pipeline Regs
typedef struct {
    bool        valid;
    Instruction instr;
    uint32_t    pc;
    uint32_t    val_rs;
    uint32_t    val_rt;
    uint32_t    aluResult;
} PipeReg;

// Core
typedef struct {
    int         core_id;
    uint32_t    pc;
    uint32_t    regfile[REGFILE_SIZE];

    PipeReg     IF_ID;
    PipeReg     ID_EX;
    PipeReg     EX_MEM;
    PipeReg     MEM_WB;

    Instruction imem[MAX_IMEM_SIZE];
    int         imem_size;

    Cache       dcache;

    bool        decode_stall;
    bool        mem_stall;
    bool        new_mem_stall;

    bool        branchTaken;
    uint32_t    branchTarget;

    bool        halted;

    bool        WB_done;
    unsigned int WB_reg;

    unsigned long cycleCount;
    unsigned long instrCount;
    unsigned long read_hit;
    unsigned long write_hit;
    unsigned long read_miss;
    unsigned long write_miss;
    unsigned long decode_stall_count;
    unsigned long mem_stall_count;
    unsigned long hazard_RAW_count;
    unsigned long hazard_WAR_count;
    unsigned long hazard_WAW_count;

    bool        dirty_flush;
    int         req_addr;
    int         req_type;
} Core;

// Bus transactions
typedef struct {
    bool     active;
    bool     wait_for_ans;
    int      ans_delay;
    bool     mid_flush;
    int      flush_index;
    int      cmd;
    int      originCore;
    int      destCore;
    uint32_t addr;
    uint32_t block[DCACHE_BLOCK_SIZE];
    bool     shared;
    bool     ready_to_share;
    int      cyclesLeft;
} BusTransaction;


void parse_args(int argc, char* argv[],
    char** imemFile0, char** imemFile1, char** imemFile2, char** imemFile3,
    char** meminFile,
    char** memoutFile,
    char** regoutFile0, char** regoutFile1,
    char** regoutFile2, char** regoutFile3,
    char** coretraceFile0, char** coretraceFile1,
    char** coretraceFile2, char** coretraceFile3,
    char** bustraceFile,
    char** dsramFile0, char** dsramFile1,
    char** dsramFile2, char** dsramFile3,
    char** tsramFile0, char** tsramFile1,
    char** tsramFile2, char** tsramFile3,
    char** statsFile0, char** statsFile1,
    char** statsFile2, char** statsFile3);

void initialize_sim(char* imemFiles[4], char* meminFile);
void load_imem(Core* c, const char* filename);
void load_memin(const char* filename);

void open_trace_files(char* coretraceFiles[4], char* bustraceFile);
void close_trace_files(void);

void run_sim(void);
bool cores_halted(void);
bool pipeline_drained(Core* c);

void fetch_stage(Core* c);
void decode_stage(Core* c);
void execute_stage(Core* c);
void mem_stage(Core* c);
void wb_stage(Core* c);

uint32_t do_alu(Core* c, Instruction in, uint32_t a, uint32_t b);

bool check_data_hazard(Core* c);
void insert_decode_stall(Core* c);

uint32_t read_from_cache(Core* c, uint32_t addr, bool* hit);
void write_to_cache(Core* c, uint32_t addr, uint32_t val, bool* hit, bool writeAllocate);

void request_bus_transaction(int cmd, int origin, uint32_t addr);
static void arbitrate_bus_requests(void);
void bus_cycle(void);
void bus_snoop_all_cores(int origin, int dest, int cmd, uint32_t addr);
void finish_bus_transaction(void);

void trace_core_cycle(Core* c, unsigned long cycle);
void trace_bus_cycle(unsigned long cycle, BusTransaction* t);

void dump_results(char* memoutFile, char* regoutFiles[4],
    char* dsramFiles[4], char* tsramFiles[4],
    char* statsFiles[4]);





#endif // SIM_H