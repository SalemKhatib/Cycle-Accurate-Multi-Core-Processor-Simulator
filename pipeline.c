#define _CRT_SECURE_NO_WARNINGS
#include "sim.h"




// Pipeline stages
void fetch_stage(Core* c)
{
    c->WB_done = false;

    if (c->decode_stall) {
        return;
    }

    if (!c->IF_ID.valid) {
        if (c->pc < (uint32_t)c->imem_size) {
            c->IF_ID.instr = c->imem[c->pc];
        }
        else {
            Instruction nop;
            nop.raw = 0;
            c->IF_ID.instr = nop;
        }

        c->IF_ID.pc = c->pc;
        c->IF_ID.valid = true;

        if (c->branchTaken) {
            c->pc = c->branchTarget;
            c->branchTaken = false;
        }
        else {
            c->pc++;
        }
    }
}

// Hazard detection
bool check_data_hazard(Core* c)
{
    if (!c->IF_ID.valid) return false;

    Instruction idInstr = c->IF_ID.instr;
    int src1 = idInstr.fields.rs;
    int src2 = idInstr.fields.rt;
    int src3 = idInstr.fields.rd;

    bool src3_valid =
        (idInstr.fields.opcode == OPCODE_BEQ) || (idInstr.fields.opcode == OPCODE_BNE) ||
        (idInstr.fields.opcode == OPCODE_BLT) || (idInstr.fields.opcode == OPCODE_BGT) ||
        (idInstr.fields.opcode == OPCODE_BLE) || (idInstr.fields.opcode == OPCODE_BGE) ||
        (idInstr.fields.opcode == OPCODE_JAL) || (idInstr.fields.opcode == OPCODE_SW);

    bool ex_valid = c->EX_MEM.valid;
    bool mem_valid = c->MEM_WB.valid;

    int ex_dest = -1, mem_dest = -1;

    if (ex_valid) {
        Instruction exInstr = c->EX_MEM.instr;
        switch (exInstr.fields.opcode) {
        case OPCODE_ADD: case OPCODE_SUB: case OPCODE_AND: case OPCODE_OR:
        case OPCODE_XOR: case OPCODE_MUL: case OPCODE_SLL: case OPCODE_SRA:
        case OPCODE_SRL: case OPCODE_LW:
            ex_dest = exInstr.fields.rd;
            break;
        case OPCODE_JAL:
            ex_dest = 15;
            break;
        default:
            break;
        }
    }

    if (mem_valid) {
        Instruction memInstr = c->MEM_WB.instr;
        switch (memInstr.fields.opcode) {
        case OPCODE_ADD: case OPCODE_SUB: case OPCODE_AND: case OPCODE_OR:
        case OPCODE_XOR: case OPCODE_MUL: case OPCODE_SLL: case OPCODE_SRA:
        case OPCODE_SRL: case OPCODE_LW:
            mem_dest = memInstr.fields.rd;
            break;
        case OPCODE_JAL:
            mem_dest = 15;
            break;
        default:
            break;
        }
    }

    bool hazardRAW = false;

    if (ex_valid && ex_dest > 0 &&
        (src1 == ex_dest || src2 == ex_dest || (src3_valid && (src3 == ex_dest)))) {
        hazardRAW = true;
    }

    if (!c->mem_stall && mem_valid && mem_dest > 0 &&
        (src1 == mem_dest || src2 == mem_dest || (src3_valid && (src3 == mem_dest)))) {
        hazardRAW = true;
    }

    if (c->WB_done &&
        (src1 == (int)c->WB_reg || src2 == (int)c->WB_reg || (src3_valid && (src3 == (int)c->WB_reg)))) {
        hazardRAW = true;
    }

    if (hazardRAW) {
        if (!c->mem_stall) {
            c->hazard_RAW_count++;
        }
        return true;
    }

    return false;
}

void insert_decode_stall(Core* c)
{
    c->decode_stall = true;
    if (!c->mem_stall) {
        c->decode_stall_count++;
    }
}


void decode_stage(Core* c)
{
    c->decode_stall = false;

    if (!c->ID_EX.valid && c->IF_ID.valid) {
        if (check_data_hazard(c)) {
            insert_decode_stall(c);
            return;
        }

        c->ID_EX = c->IF_ID;
        c->IF_ID.valid = false;

        Instruction in = c->ID_EX.instr;
        int rs = in.fields.rs;
        int rt = in.fields.rt;

        int16_t sx = (int16_t)(in.fields.imm & 0x0FFFu);
        if (sx & 0x0800) {
            sx |= (int16_t)0xF000;
        }
        c->regfile[1] = (uint32_t)sx;

        uint32_t val_rs = (rs == 0 ? 0u : c->regfile[rs]);
        uint32_t val_rt = (rt == 0 ? 0u : c->regfile[rt]);

        c->ID_EX.val_rs = val_rs;
        c->ID_EX.val_rt = val_rt;

        c->branchTaken = false;

        switch (in.fields.opcode) {
        case OPCODE_BEQ:
            if (val_rs == val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_BNE:
            if (val_rs != val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_BLT:
            if ((int32_t)val_rs < (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_BGT:
            if ((int32_t)val_rs > (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_BLE:
            if ((int32_t)val_rs <= (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_BGE:
            if ((int32_t)val_rs >= (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            }
            break;
        case OPCODE_JAL:
            c->branchTaken = true;
            c->branchTarget = c->regfile[in.fields.rd] & 0x3FFu;
            break;
        default:
            break;
        }
    }
}


void execute_stage(Core* c)
{
    if (!c->EX_MEM.valid && c->ID_EX.valid) {
        c->EX_MEM = c->ID_EX;
        c->ID_EX.valid = false;

        Instruction in = c->EX_MEM.instr;
        uint32_t a = c->EX_MEM.val_rs;
        uint32_t b = c->EX_MEM.val_rt;

        c->EX_MEM.aluResult = do_alu(c, in, a, b);
    }
}

uint32_t do_alu(Core* c, Instruction in, uint32_t a, uint32_t b)
{
    (void)c;
    switch (in.fields.opcode) {
    case OPCODE_ADD: return a + b;
    case OPCODE_SUB: return a - b;
    case OPCODE_AND: return a & b;
    case OPCODE_OR:  return a | b;
    case OPCODE_XOR: return a ^ b;
    case OPCODE_MUL: return a * b;
    case OPCODE_SLL: return a << (b & 31u);
    case OPCODE_SRA: {
        int32_t sa = (int32_t)a;
        return (uint32_t)(sa >> (b & 31u));
    }
    case OPCODE_SRL: return a >> (b & 31u);
    case OPCODE_LW:
    case OPCODE_SW:  return a + b;
    default:         return 0;
    }
}

void mem_stage(Core* c)
{
    if (!c->MEM_WB.valid && c->EX_MEM.valid) {

        if (c->mem_stall) {
            c->mem_stall_count++;
            return;
        }

        Instruction in = c->EX_MEM.instr;
        uint32_t addr = c->EX_MEM.aluResult;

        switch (in.fields.opcode) {
        case OPCODE_LW: {
            bool hit = false;
            uint32_t data = read_from_cache(c, addr, &hit);

            if (!hit) {
                c->mem_stall = true;
                c->new_mem_stall = true;
                c->mem_stall_count++;
            }
            else {
                c->MEM_WB = c->EX_MEM;
                c->EX_MEM.valid = false;
                c->MEM_WB.aluResult = data;
            }
        } break;

        case OPCODE_SW: {
            bool hit = false;
            int rd = in.fields.rd;
            uint32_t val = c->regfile[rd];

            write_to_cache(c, addr, val, &hit, true);

            if (!hit) {
                c->mem_stall = true;
                c->new_mem_stall = true;
                c->mem_stall_count++;
            }
            else {
                c->MEM_WB = c->EX_MEM;
                c->EX_MEM.valid = false;
            }
        } break;

        case OPCODE_HALT:
            c->MEM_WB = c->EX_MEM;
            c->EX_MEM.valid = false;
            break;

        default:
            c->MEM_WB = c->EX_MEM;
            c->EX_MEM.valid = false;
            break;
        }
    }
}


void wb_stage(Core* c)
{
    if (!c->MEM_WB.valid) return;


    if (c->core_id == 2) {
        c->core_id = 2;
    }

    Instruction in = c->MEM_WB.instr;
    int rd = in.fields.rd;

    switch (in.fields.opcode) {
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    case OPCODE_MUL:
    case OPCODE_SLL:
    case OPCODE_SRA:
    case OPCODE_SRL:
    case OPCODE_LW:
        if (rd != 0 && rd != 1) {
            c->regfile[rd] = c->MEM_WB.aluResult;
        }
        c->instrCount++;
        c->WB_done = true;
        c->WB_reg = (unsigned int)rd;
        break;

    case OPCODE_SW:
    case OPCODE_BEQ:
    case OPCODE_BNE:
    case OPCODE_BLT:
    case OPCODE_BGT:
    case OPCODE_BLE:
    case OPCODE_BGE:
        c->instrCount++;
        break;

    case OPCODE_JAL:
        c->regfile[15] = c->MEM_WB.pc + 1;
        c->instrCount++;
        c->WB_done = true;
        c->WB_reg = (unsigned int)rd;
        break;

    case OPCODE_HALT:
        c->instrCount++;
        c->halted = true;
        break;

    default:
        break;
    }

    c->MEM_WB.valid = false;
}

bool pipeline_drained(Core* c)
{
    return (!c->halted || c->IF_ID.valid || c->ID_EX.valid || c->EX_MEM.valid || c->MEM_WB.valid) ? false : true;
}