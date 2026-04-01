#define _CRT_SECURE_NO_WARNINGS
#include "sim.h"


static Core cores[NUM_CORES];
static uint32_t mainMem[MAIN_MEM_SIZE];

static BusTransaction busTxn;

static bool     busRequest[NUM_CORES];
static int      busCmdReq[NUM_CORES];
static uint32_t busAddrReq[NUM_CORES];
static int      priority_list[4] = { 0, 1, 2, 3 };

static FILE* coreTraceFiles[NUM_CORES] = { NULL, NULL, NULL, NULL };
static FILE* busTraceFile = NULL;



// Helper functions
static inline uint32_t align_block_base(uint32_t addr)
{
    return (addr >> 3) << 3; // floor to 8-word block
}

static void writeback_block_to_mainmem(uint32_t baseAddr, const uint32_t block[DCACHE_BLOCK_SIZE])
{
    for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
        uint32_t a = baseAddr + (uint32_t)i;
        if (a < MAIN_MEM_SIZE) {
            mainMem[a] = block[i];
        }
    }
}



int main(int argc, char* argv[])
{
    if (argc < 28) {
        fprintf(stderr,
            "Usage: %s imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt "
            "regout0.txt regout1.txt regout2.txt regout3.txt "
            "coretrace0.txt coretrace1.txt coretrace2.txt coretrace3.txt "
            "bustrace.txt "
            "dsram0.txt dsram1.txt dsram2.txt dsram3.txt "
            "tsram0.txt tsram1.txt tsram2.txt tsram3.txt "
            "stats0.txt stats1.txt stats2.txt stats3.txt\n",
            argv[0]);
        return 1;
    }

    char* imemFile0, * imemFile1, * imemFile2, * imemFile3;
    char* meminFile, * memoutFile;
    char* regoutFile0, * regoutFile1, * regoutFile2, * regoutFile3;
    char* coretraceFile0, * coretraceFile1, * coretraceFile2, * coretraceFile3;
    char* bustraceFile;
    char* dsramFile0, * dsramFile1, * dsramFile2, * dsramFile3;
    char* tsramFile0, * tsramFile1, * tsramFile2, * tsramFile3;
    char* statsFile0, * statsFile1, * statsFile2, * statsFile3;

    parse_args(argc, argv,
        &imemFile0, &imemFile1, &imemFile2, &imemFile3,
        &meminFile,
        &memoutFile,
        &regoutFile0, &regoutFile1, &regoutFile2, &regoutFile3,
        &coretraceFile0, &coretraceFile1, &coretraceFile2, &coretraceFile3,
        &bustraceFile,
        &dsramFile0, &dsramFile1, &dsramFile2, &dsramFile3,
        &tsramFile0, &tsramFile1, &tsramFile2, &tsramFile3,
        &statsFile0, &statsFile1, &statsFile2, &statsFile3);

    char* imemFiles[4] = { imemFile0, imemFile1, imemFile2, imemFile3 };
    initialize_sim(imemFiles, meminFile);

    char* coretraceFiles[4] = { coretraceFile0, coretraceFile1, coretraceFile2, coretraceFile3 };
    open_trace_files(coretraceFiles, bustraceFile);

    run_sim();

    char* regoutFiles[4] = { regoutFile0, regoutFile1, regoutFile2, regoutFile3 };
    char* dsramFiles[4] = { dsramFile0, dsramFile1, dsramFile2, dsramFile3 };
    char* tsramFiles[4] = { tsramFile0, tsramFile1, tsramFile2, tsramFile3 };
    char* statsFiles[4] = { statsFile0, statsFile1, statsFile2, statsFile3 };

    dump_results(memoutFile, regoutFiles, dsramFiles, tsramFiles, statsFiles);

    close_trace_files();
    return 0;
}

// Parse arguments
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
    char** statsFile2, char** statsFile3)
{
    (void)argc;
    *imemFile0 = argv[1];
    *imemFile1 = argv[2];
    *imemFile2 = argv[3];
    *imemFile3 = argv[4];
    *meminFile = argv[5];
    *memoutFile = argv[6];
    *regoutFile0 = argv[7];
    *regoutFile1 = argv[8];
    *regoutFile2 = argv[9];
    *regoutFile3 = argv[10];
    *coretraceFile0 = argv[11];
    *coretraceFile1 = argv[12];
    *coretraceFile2 = argv[13];
    *coretraceFile3 = argv[14];
    *bustraceFile = argv[15];
    *dsramFile0 = argv[16];
    *dsramFile1 = argv[17];
    *dsramFile2 = argv[18];
    *dsramFile3 = argv[19];
    *tsramFile0 = argv[20];
    *tsramFile1 = argv[21];
    *tsramFile2 = argv[22];
    *tsramFile3 = argv[23];
    *statsFile0 = argv[24];
    *statsFile1 = argv[25];
    *statsFile2 = argv[26];
    *statsFile3 = argv[27];
}

// Intialization
void initialize_sim(char* imemFiles[4], char* meminFile)
{
    for (int i = 0; i < MAIN_MEM_SIZE; i++) {
        mainMem[i] = 0;
    }
    load_memin(meminFile);

    busTxn.active = false;
    busTxn.wait_for_ans = false;
    busTxn.ans_delay = 0;
    busTxn.mid_flush = false;
    busTxn.flush_index = 0;
    busTxn.cmd = BUS_NONE;
    busTxn.originCore = -1;
    busTxn.destCore = -1;
    busTxn.addr = 0;
    busTxn.shared = false;
    busTxn.ready_to_share = false;
    busTxn.cyclesLeft = 0;
    for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
        busTxn.block[w] = 0;
    }

    for (int c = 0; c < NUM_CORES; c++) {
        busRequest[c] = false;
        busCmdReq[c] = BUS_NONE;
        busAddrReq[c] = 0;
    }

    for (int c = 0; c < NUM_CORES; c++) {
        cores[c].core_id = c;
        cores[c].pc = 0;
        load_imem(&cores[c], imemFiles[c]);

        for (int r = 0; r < REGFILE_SIZE; r++) {
            cores[c].regfile[r] = 0;
        }

        cores[c].IF_ID.valid = false;
        cores[c].ID_EX.valid = false;
        cores[c].EX_MEM.valid = false;
        cores[c].MEM_WB.valid = false;

        cores[c].halted = false;
        cores[c].decode_stall = false;
        cores[c].mem_stall = false;
        cores[c].new_mem_stall = false;
        cores[c].branchTaken = false;
        cores[c].branchTarget = 0;
        cores[c].WB_done = false;
        cores[c].WB_reg = 0;
        cores[c].dirty_flush = false;

        cores[c].cycleCount = 0;
        cores[c].instrCount = 0;
        cores[c].read_hit = 0;
        cores[c].write_hit = 0;
        cores[c].read_miss = 0;
        cores[c].write_miss = 0;
        cores[c].decode_stall_count = 0;
        cores[c].mem_stall_count = 0;
        cores[c].hazard_RAW_count = 0;
        cores[c].hazard_WAR_count = 0;
        cores[c].hazard_WAW_count = 0;

        for (int i = 0; i < DCACHE_NUM_LINES; i++) {
            cores[c].dcache.lines[i].mesi = MESI_INVALID;
            cores[c].dcache.lines[i].tag = 0;
            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                cores[c].dcache.lines[i].block[w] = 0;
            }
        }
    }
}

// Load imem0 - imem3
void load_imem(Core* c, const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Can't open IMEM file: %s\n", filename);
        exit(1);
    }

    char line[128];
    int index = 0;

    while (fgets(line, sizeof(line), f)) {
        uint32_t val;
        if (sscanf(line, "%x", &val) == 1) {
            c->imem[index].raw = val;
            c->imem[index].fields.imm = (val & 0xFFFu);
            c->imem[index].fields.rt = (val >> 12) & 0xFu;
            c->imem[index].fields.rs = (val >> 16) & 0xFu;
            c->imem[index].fields.rd = (val >> 20) & 0xFu;
            c->imem[index].fields.opcode = (val >> 24) & 0xFFu;

            index++;
            if (index >= MAX_IMEM_SIZE) {
                break;
            }
        }
    }

    c->imem_size = index;
    fclose(f);
}

// Load memin.txt
void load_memin(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Can't open memin file: %s\n", filename);
        exit(1);
    }

    char line[128];
    int index = 0;

    while (fgets(line, sizeof(line), f)) {
        uint32_t val;
        if (sscanf(line, "%x", &val) == 1) {
            if (index < MAIN_MEM_SIZE) {
                mainMem[index] = val;
                index++;
            }
        }
    }

    fclose(f);
}

// Trace files - open and close
void open_trace_files(char* coretraceFiles[4], char* bustraceFile)
{
    for (int c = 0; c < NUM_CORES; c++) {
        coreTraceFiles[c] = fopen(coretraceFiles[c], "w");
        if (!coreTraceFiles[c]) {
            fprintf(stderr, "Error: Can't open core trace file: %s\n", coretraceFiles[c]);
            exit(1);
        }
    }

    busTraceFile = fopen(bustraceFile, "w");
    if (!busTraceFile) {
        fprintf(stderr, "Error: Can't open bus trace file: %s\n", bustraceFile);
        exit(1);
    }
}

void close_trace_files(void)
{
    for (int c = 0; c < NUM_CORES; c++) {
        if (coreTraceFiles[c]) {
            fclose(coreTraceFiles[c]);
            coreTraceFiles[c] = NULL;
        }
    }
    if (busTraceFile) {
        fclose(busTraceFile);
        busTraceFile = NULL;
    }
}

// Simulation loop
void run_sim(void)
{
    unsigned long global_cycle = 0;
    const unsigned long MAX_CYCLES = 2000000UL;

    while (!cores_halted() && global_cycle < MAX_CYCLES) {

        for (int c = 0; c < NUM_CORES; c++) {
            trace_core_cycle(&cores[c], global_cycle);
        }

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                wb_stage(&cores[c]);
            }
        }

        arbitrate_bus_requests();
        bus_cycle();

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                mem_stage(&cores[c]);
            }
        }

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                execute_stage(&cores[c]);
            }
        }

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                decode_stage(&cores[c]);
            }
        }

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                fetch_stage(&cores[c]);
            }
        }

        if (busTxn.active || busTxn.mid_flush) {
            trace_bus_cycle(global_cycle, &busTxn);
            busTxn.active = false;
        }

        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                cores[c].cycleCount++;
            }
        }

        global_cycle++;
    }
}

// End of simulation condition - cores halted and pipelines drained
bool cores_halted(void)
{
    for (int c = 0; c < NUM_CORES; c++) {
        if (!cores[c].halted || !pipeline_drained(&cores[c])) {
            return false;
        }
    }
    return true;
}







void request_bus_transaction(int cmd, int origin, uint32_t addr)
{
    busRequest[origin] = true;
    busCmdReq[origin] = cmd;
    busAddrReq[origin] = addr;
}


uint32_t read_from_cache(Core* c, uint32_t addr, bool* hit)
{
    uint32_t idx = get_indx(addr);
    uint32_t tag = get_tag(addr);
    uint32_t off = get_offset(addr);

    CacheLine* line = &c->dcache.lines[idx];

    if (line->mesi != MESI_INVALID && line->tag == tag) {
        *hit = true;
        c->read_hit++;
        return line->block[off];
    }

    *hit = false;
    c->read_miss++;

    // Miss handling 
    if (line->mesi == MESI_MODIFIED) {
        if (!c->dirty_flush) {
            int baseAddr = (((line->tag << 9) + (idx << 3)) >> 3) << 3;

            c->req_addr = addr;
            request_bus_transaction(BUS_FLUSH, c->core_id, (uint32_t)baseAddr);
            c->dirty_flush = true;
            c->req_type = BUS_RD;

            // Write back dirty block to main memory
            writeback_block_to_mainmem((uint32_t)baseAddr, line->block);
        }
        else {
            c->dirty_flush = false;
        }

    }
    else {
        request_bus_transaction(BUS_RD, c->core_id, addr);
    }

    return 0;
}

void write_to_cache(Core* c, uint32_t addr, uint32_t val, bool* hit, bool writeAllocate)
{
    (void)writeAllocate;

    uint32_t idx = get_indx(addr);
    uint32_t tag = get_tag(addr);
    uint32_t off = get_offset(addr);

    CacheLine* line = &c->dcache.lines[idx];

    if (line->mesi != MESI_INVALID && line->mesi != MESI_SHARED && line->tag == tag) {
        *hit = true;
        c->write_hit++;

        if (line->mesi == MESI_EXCLUSIVE) {
            line->mesi = MESI_MODIFIED;
        }
        line->block[off] = val;
        return;
    }

    *hit = false;
    c->write_miss++;

    if (line->mesi != MESI_MODIFIED) {
        request_bus_transaction(BUS_RDX, c->core_id, addr);
        line->mesi = MESI_MODIFIED;
    }
    else {
        if (!c->dirty_flush) {
            int baseaddr = (((line->tag << 9) + (idx << 3)) >> 3) << 3;

            c->req_addr = addr;
            request_bus_transaction(BUS_FLUSH, c->core_id, (uint32_t)baseaddr);

            // writeback dirty block (same loop)
            writeback_block_to_mainmem((uint32_t)baseaddr, line->block);

            c->dirty_flush = true;
            c->req_type = BUS_RDX;

        }
        else {
            c->dirty_flush = false;
        }
    }
}




// Round Robin bus arbitration
static void arbitrate_bus_requests(void)
{
    int granted_id = -1;
    bool flush_core = false;

    busTxn.active = false;

    for (int i = 0; i < NUM_CORES; i++) {
        int chosen = priority_list[i];

        if (cores[chosen].new_mem_stall) {
            cores[chosen].new_mem_stall = false;
            continue;
        }

        if (busRequest[chosen] && !busTxn.wait_for_ans && !busTxn.mid_flush) {

            busTxn.wait_for_ans = true;
            busTxn.cyclesLeft = MAIN_MEMORY_DELAY + DCACHE_BLOCK_SIZE - 1; // 16 + 7
            busRequest[chosen] = false;

            if (busCmdReq[chosen] != BUS_FLUSH) {
                busTxn.active = true;
            }
            else {
                flush_core = true;
                busTxn.mid_flush = true;
                busTxn.flush_index = 0;
                busTxn.cyclesLeft = DCACHE_BLOCK_SIZE - 1;
                busTxn.wait_for_ans = false;
            }

            busTxn.cmd = busCmdReq[chosen];
            busTxn.addr = busAddrReq[chosen];
            busTxn.originCore = chosen;
            busTxn.destCore = busTxn.originCore;
            busTxn.shared = false;
            busTxn.ready_to_share = false;

            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                busTxn.block[w] = 0;
            }

            granted_id = i;
            break;
        }
        else if (busTxn.wait_for_ans &&
            (busCmdReq[chosen] == BUS_FLUSH) &&
            busRequest[chosen] &&
            ((((busAddrReq[chosen] >> 3) << 3) == ((busTxn.addr >> 3) << 3)))) {

            busRequest[chosen] = false;

            busTxn.wait_for_ans = false;
            busTxn.mid_flush = true;
            busTxn.ready_to_share = false;
            flush_core = true;

            busTxn.flush_index = 0;
            busTxn.shared = (busTxn.cmd == BUS_RD);
            busTxn.cyclesLeft = DCACHE_BLOCK_SIZE - 1;
            busTxn.addr = busAddrReq[chosen];
            busTxn.cmd = busCmdReq[chosen];
            busTxn.destCore = busTxn.originCore;
            busTxn.originCore = chosen;
        }
    }

    if ((busTxn.wait_for_ans || busTxn.mid_flush) &&
        busTxn.cyclesLeft < (DCACHE_BLOCK_SIZE) &&
        !flush_core) {

        if (busTxn.wait_for_ans) {
            busTxn.shared = busTxn.ready_to_share;
            busTxn.ready_to_share = false;

            busTxn.wait_for_ans = false;
            busTxn.mid_flush = true;
            busTxn.flush_index = 0;
            busTxn.addr = (busTxn.addr >> 3) << 3;
            busTxn.cmd = BUS_FLUSH;
            busTxn.destCore = busTxn.originCore;
            busTxn.originCore = 4;
        }
        else {
            busTxn.flush_index++;
            busTxn.addr++;
            if (busTxn.flush_index == DCACHE_BLOCK_SIZE) {
                busTxn.mid_flush = false;
            }
        }
    }

    if (granted_id != -1) {
        int granted_core = priority_list[granted_id];
        for (int i = granted_id; i < 3; i++) {
            priority_list[i] = priority_list[i + 1];
        }
        priority_list[3] = granted_core;
    }
}

// Bus cycle
void bus_cycle(void)
{
    if (!busTxn.active && !busTxn.wait_for_ans && !busTxn.mid_flush) {
        return;
    }

    if (busTxn.active) {
        bus_snoop_all_cores(busTxn.originCore, busTxn.destCore, busTxn.cmd, busTxn.addr);
    }

    if (busTxn.cyclesLeft == (DCACHE_BLOCK_SIZE - 1)) {
        uint32_t baseAddr = align_block_base(busTxn.addr);
        baseAddr &= 0xFFFF8u;

        for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
            uint32_t a = baseAddr + (uint32_t)i;
            if (a < MAIN_MEM_SIZE) {
                busTxn.block[i] = mainMem[a];
            }
        }
    }

    if (busTxn.cyclesLeft <= 0 || busTxn.active) {
        finish_bus_transaction();
    }

    busTxn.cyclesLeft--;
}

// Snooping all cores
void bus_snoop_all_cores(int origin, int dest, int cmd, uint32_t addr)
{
    for (int c = 0; c < NUM_CORES; c++) {

        if (c == dest || c == origin) {
            continue;
        }

        uint32_t idx = get_indx(addr);
        uint32_t tag = get_tag(addr);

        CacheLine* line = &cores[c].dcache.lines[idx];

        if (line->mesi != MESI_INVALID && line->tag == tag) {

            if (cmd == BUS_RD) {
                busTxn.ready_to_share = true;

                if (line->mesi == MESI_MODIFIED) {
                    uint32_t baseAddr = align_block_base(addr);

                    request_bus_transaction(BUS_FLUSH, cores[c].core_id, baseAddr);
                    writeback_block_to_mainmem(baseAddr, line->block);

                    line->mesi = MESI_SHARED;
                }
                else if (line->mesi == MESI_EXCLUSIVE) {
                    line->mesi = MESI_SHARED;
                }
            }
            else if (cmd == BUS_RDX) {
                if (line->mesi == MESI_MODIFIED) {
                    uint32_t baseAddr = align_block_base(addr);

                    request_bus_transaction(BUS_FLUSH, cores[c].core_id, baseAddr);
                    writeback_block_to_mainmem(baseAddr, line->block);
                }
                line->mesi = MESI_INVALID;
            }
        }
    }
}

// finish bus transaction
void finish_bus_transaction(void)
{
    int origin = busTxn.destCore;
    int cmd = busTxn.cmd;
    uint32_t addr = busTxn.addr;

    Core* c = &cores[origin];

    uint32_t idx = get_indx(addr);
    uint32_t tag = get_tag(addr);

    CacheLine* line = &c->dcache.lines[idx];

    if (cmd == BUS_RD || cmd == BUS_RDX) {
        line->tag = (uint16_t)tag;

        if (cmd == BUS_RD) {
            line->mesi = (busTxn.ready_to_share ? MESI_SHARED : MESI_EXCLUSIVE);
        }
        else {
            line->mesi = MESI_MODIFIED;
        }
    }
    else {
        if (c->dirty_flush) {
            request_bus_transaction(c->req_type, c->core_id, (uint32_t)c->req_addr);
        }

        for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
            line->block[i] = busTxn.block[i];
        }

        c->mem_stall = false;
    }
}

// Traces for bus and cores
void trace_core_cycle(Core* c, unsigned long cycle)
{
    char fetch_str[16] = "---";
    char decode_str[16] = "---";
    char exec_str[16] = "---";
    char mem_str[16] = "---";
    char wb_str[16] = "---";

    if (c->ID_EX.instr.fields.opcode != OPCODE_HALT) {
        snprintf(fetch_str, 16, "%03X", c->pc);
    }

    if (c->IF_ID.valid && c->ID_EX.instr.fields.opcode != OPCODE_HALT) {
        snprintf(decode_str, 16, "%03X", c->IF_ID.pc);
    }

    if (c->ID_EX.valid && c->EX_MEM.instr.fields.opcode != OPCODE_HALT) {
        snprintf(exec_str, 16, "%03X", c->ID_EX.pc);
    }

    if (c->EX_MEM.valid && c->MEM_WB.instr.fields.opcode != OPCODE_HALT) {
        snprintf(mem_str, 16, "%03X", c->EX_MEM.pc);
    }

    if (c->MEM_WB.valid) {
        snprintf(wb_str, 16, "%03X", c->MEM_WB.pc);
    }

    if (!c->halted) {
        fprintf(coreTraceFiles[c->core_id],
            "%lu %s %s %s %s %s",
            cycle, fetch_str, decode_str, exec_str, mem_str, wb_str);

        for (int r = 2; r < REGFILE_SIZE; r++) {
            fprintf(coreTraceFiles[c->core_id], " %08X", c->regfile[r]);
        }
        fprintf(coreTraceFiles[c->core_id], " \n");
    }
}


void trace_bus_cycle(unsigned long cycle, BusTransaction* t)
{
    fprintf(busTraceFile,
        "%lu %d %d %05X %08X %d\n",
        cycle,
        t->originCore,
        t->cmd,
        (t->addr & 0xFFFFF),
        (t->cmd != BUS_FLUSH) ? 0U : t->block[t->addr & 0x7],
        t->shared ? 1 : 0);
}

// Results dumping
void dump_results(char* memoutFile, char* regoutFiles[4],
    char* dsramFiles[4], char* tsramFiles[4],
    char* statsFiles[4])
{
    int last_index = -1;

    FILE* fmemout = fopen(memoutFile, "w");
    if (!fmemout) {
        fprintf(stderr, "Error: Can't open memout file %s\n", memoutFile);
        exit(1);
    }

    for (int i = 0; i < MAIN_MEM_SIZE; i++) {
        if (mainMem[i] != 0) {
            last_index = i + 1;
        }
    }

    for (int i = 0; i < last_index; i++) {
        fprintf(fmemout, "%08X\n", mainMem[i]);
    }
    fclose(fmemout);

    for (int c = 0; c < NUM_CORES; c++) {
        FILE* freg = fopen(regoutFiles[c], "w");
        if (!freg) {
            fprintf(stderr, "Error: Can't open regout file %s\n", regoutFiles[c]);
            exit(1);
        }

        for (int r = 2; r < REGFILE_SIZE; r++) {
            fprintf(freg, "%08X\n", cores[c].regfile[r]);
        }
        fclose(freg);
    }

    for (int c = 0; c < NUM_CORES; c++) {
        FILE* fds = fopen(dsramFiles[c], "w");
        FILE* fts = fopen(tsramFiles[c], "w");

        if (!fds || !fts) {
            fprintf(stderr, "Error: Can't open dsram/tsram files\n");
            exit(1);
        }

        for (int i = 0; i < DCACHE_NUM_LINES; i++) {
            CacheLine* line = &cores[c].dcache.lines[i];

            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                fprintf(fds, "%08X\n", line->block[w]);
            }

            uint32_t mesi = (line->mesi & 0x3u);
            uint32_t ts = ((mesi << 12) | (line->tag & 0xFFFu));
            fprintf(fts, "%08X\n", ts);
        }

        fclose(fds);
        fclose(fts);
    }

    for (int c = 0; c < NUM_CORES; c++) {
        FILE* fs = fopen(statsFiles[c], "w");
        if (!fs) {
            fprintf(stderr, "Error: Can't open stats file %s\n", statsFiles[c]);
            exit(1);
        }

        fprintf(fs, "cycles %lu\n", cores[c].cycleCount + 1);
        fprintf(fs, "instructions %lu\n", cores[c].instrCount);


        fprintf(fs, "read_hit %lu\n", cores[c].read_hit - cores[c].read_miss);
        fprintf(fs, "write_hit %lu\n", cores[c].write_hit - cores[c].write_miss);

        fprintf(fs, "read_miss %lu\n", cores[c].read_miss);
        fprintf(fs, "write_miss %lu\n", cores[c].write_miss);
        fprintf(fs, "decode_stall %lu\n", cores[c].decode_stall_count);
        fprintf(fs, "mem_stall %lu\n", cores[c].mem_stall_count);

        fclose(fs);
    }
}