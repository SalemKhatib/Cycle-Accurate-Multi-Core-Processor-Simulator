# Cycle-Accurate Multi-Core Processor Simulator

## Overview
A cycle-accurate multi-core processor simulator implemented in C. This project models a 4-core system interconnected via a centralized, round-robin arbitrated shared bus to a main memory, utilizing the strict MESI (Modified, Exclusive, Shared, Invalid) protocol for L1 data cache coherence. 

This simulator bridges the gap between software engineering and hardware architecture, demonstrating low-level C programming, memory management, and deep understanding of CPU pipelines and state machines.

## My Contributions: Pipeline Architecture (`pipeline.c`)
I owned the design and implementation of the core pipeline execution logic and hardware hazard management system. 

* **5-Stage Pipeline Implementation:** Developed the logic for the classic RISC pipeline stages: Fetch (IF), Decode (ID), Execute (EX), Memory (MEM), and Writeback (WB).
* **Cycle-Accurate Register Latching:** Engineered the simulation flow to process pipeline stages in reverse order (WB → MEM → EX → ID → IF) within each clock cycle to correctly simulate physical hardware register latching behavior.
* **Hazard Detection Unit:** Implemented robust data hazard detection. Handled Read-After-Write (RAW) dependencies via a dynamic stalling mechanism that freezes the Fetch and Decode stages until dependencies are safely resolved.
* **Branch Resolution:** Designed the control flow logic to resolve branch instructions during the Decode stage. Implemented a "Branch Not Taken" prediction strategy, which immediately updates the Program Counter (PC) and flushes the Fetch stage upon a taken branch.
* **ALU & Core State Integration:** Developed the arithmetic logic unit execution paths and managed the internal core state registers and pipeline buffers (`IF_ID`, `ID_EX`, `EX_MEM`, `MEM_WB`).

## System Architecture Highlights
* **Cores:** 4 Independent cores running a custom instruction set.
* **Memory Subsystem:** $2^{20}$ word main memory with a simulated 16-cycle access delay.
* **L1 Data Cache:** Direct-mapped, 64 cache lines, 8 words per block.
* **Coherence:** Strict adherence to the MESI protocol for local accesses and bus snooping.
* **Bus Arbitration:** Centralized bus with Round-Robin arbitration to ensure fair bandwidth distribution among cores.

## Compilation and Execution

### Prerequisites
* A standard C compiler (e.g., GCC or Clang)

### Build Instructions
Clone the repository and compile the source files using GCC:

```bash
gcc -O3 -Wall -Wextra -o sim sim.c pipeline.c
