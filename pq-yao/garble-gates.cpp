#include "pq-yao/garble-gates.h"

using namespace std;
using namespace emp;

// Double encryption of 256-bit label C using 256-bit keys keyA and keyB
void emp::AESNI_encrypt_label(Label& ct, const Label &C, const AESNI_KEY* keyA,
        const AESNI_KEY* keyB, uint64_t gid, uint8_t entry){
    // tweak = gid || entry
    uint64_t tweak_hi = gid;
    uint64_t tweak_lo = (entry << 2);
    block counters[4];
    counters[0] = makeBlock(tweak_hi, tweak_lo | 0);
    counters[1] = makeBlock(tweak_hi, tweak_lo | 1);
    counters[2] = makeBlock(tweak_hi, tweak_lo | 2);
    counters[3] = makeBlock(tweak_hi, tweak_lo | 3);

    /*
    ct = C \xor ((AES256(keyA, tweak || 00) \xor AES256(keyB, tweak || 10))
                 || (AES256(keyA, tweak || 01) \xor AES256(keyB, tweak || 11)))
    */
    AESNI_ecb_encrypt_blks(counters, 2, keyA);
    AESNI_ecb_encrypt_blks(counters + 2, 2, keyB);
    counters[0] = _mm_xor_si128(counters[0], counters[2]);
    counters[1] = _mm_xor_si128(counters[1], counters[3]);
    ct.lo = _mm_xor_si128(counters[0], C.lo);
    ct.hi = _mm_xor_si128(counters[1], C.hi);
}

// Encryption and Decryption are symmetric
void emp::AESNI_decrypt_label(Label& C, const Label &ct, const AESNI_KEY* keyA,
        const AESNI_KEY* keyB, uint64_t gid, uint8_t entry){
    AESNI_encrypt_label(C, ct, keyA, keyB, gid, entry);
}

// A0, B0, and C0 correspond to the labels held by the Evaluator for wires A, B, and C, resp.
void emp::garble_eval_gate(Label &C0, const Label &A0, const Label &B0,
        uint64_t gid, const Label *garbled_table) {
    // Permutation bits of A0 and B0
	uint8_t sa = getLSB(A0.lo);
	uint8_t sb = getLSB(B0.lo);

    // Table entry corresponding to A0 and B0
    uint8_t entry = sa * 2 + sb;
    Label ct = garbled_table[entry];
    AESNI_KEY keyA, keyB;

    // Generate AES key schedules for labels A0 and B0
    AESNI_set_encrypt_key(&keyA, (unsigned char*) &A0, 32);
    AESNI_set_encrypt_key(&keyB, (unsigned char*) &B0, 32);

    // Decrypt ct to get label corresponding to wire C
    AESNI_decrypt_label(C0, ct, &keyA, &keyB, gid, entry);
}

void emp::garble_gen_gate(Label &C0, Label &C1, const Label &A0, const Label &A1,
        const Label &B0, const Label &B1, uint64_t gid, Label *garbled_table,
        PRG* prg, GateType gtype) {

    assert(gtype == AND || gtype == XOR);

    // Sample random labels
    prg->random_block((block*) &C0, 2);
    prg->random_block((block*) &C1, 2);

    // Permutation bits of A0 and B0
	uint8_t sa = getLSB(A0.lo);
	uint8_t sb = getLSB(B0.lo);

    // Permutation bit of C0
    bool sc = getLSB(C0.lo);
    // Set LSB of C1 as 1 ^ sc
    if(sc) set_lsb_zero(C1.lo);
    else set_lsb_one(C1.lo);

    // Generate AES Key schedules for labels A0, A1, B0, and B1
    AESNI_KEY keyA[2], keyB[2];
    AESNI_set_encrypt_key(&keyA[0], (unsigned char*) &A0, 32);
    AESNI_set_encrypt_key(&keyA[1], (unsigned char*) &A1, 32);
    AESNI_set_encrypt_key(&keyB[0], (unsigned char*) &B0, 32);
    AESNI_set_encrypt_key(&keyB[1], (unsigned char*) &B1, 32);

    // Table of output labels with entries according to the gate type
    // table[i*2 + j] = C{i gtype j}
    Label table[4];
    if (gtype == AND) {
        table[0*2 + 0] = C0;
        table[0*2 + 1] = C0;
        table[1*2 + 0] = C0;
        table[1*2 + 1] = C1;
    } else if (gtype == XOR) {
        table[0*2 + 0] = C0;
        table[0*2 + 1] = C1;
        table[1*2 + 0] = C1;
        table[1*2 + 1] = C0;
    }

    // Place the encryptions of the table entries in the garbled table
    // according to the permutation bits of the input labels
    for (uint8_t i = 0; i < 2; i++) {
        for (uint8_t j = 0; j < 2; j++) {
            uint8_t entry = (sa ^ i) * 2 + (sb ^ j);
            AESNI_encrypt_label(garbled_table[entry], table[i*2 + j],
                    &keyA[i], &keyB[j], gid, entry);
        }
    }
}
