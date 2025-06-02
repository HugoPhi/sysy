#ifndef RV_INST_IMPL_H
#define RV_INST_IMPL_H

#include <cstdint>

#include "backend/rv_def.h"

namespace rv {
    struct rv_inst {
        rvREG rd, rs1, rs2;  // operands of rv inst
        rvOPCODE op;         // opcode of rv inst
        int imm;             // optional, in immediate inst
        std::string label;   // optional, in beq/jarl inst
    };
};  // namespace rv

#endif