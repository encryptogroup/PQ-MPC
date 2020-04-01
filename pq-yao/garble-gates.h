#ifndef GARBLE_GATES_H__
#define GARBLE_GATES_H__
#include "emp-tool/utils/prg.h"
#include "emp-tool/utils/aes-ni.h"
#include <string.h>

namespace emp {
enum GateType {AND, XOR};

void AESNI_encrypt_label(Label& ct, const Label &C, const AESNI_KEY* keyA,
        const AESNI_KEY* keyB, uint64_t gid, uint8_t entry);

void AESNI_decrypt_label(Label& C, const Label &ct, const AESNI_KEY* keyA,
        const AESNI_KEY* keyB, uint64_t gid, uint8_t entry);

void garble_eval_gate(Label &C0, const Label &A0, const Label &B0,
        uint64_t gid, const Label *garbled_table);

void garble_gen_gate(Label &C0, Label &C1, const Label &A0, const Label &A1,
        const Label &B0, const Label &B1, uint64_t gid, Label *garbled_table,
        PRG* prg, GateType gtype);
}
#endif // GARBLE_GATES_H__
