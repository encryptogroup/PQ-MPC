#ifndef GATE_GEN_H__
#define GATE_GEN_H__
#include "emp-tool/io/net-io.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/utils.h"
#include "emp-tool/execution/circuit_execution.h"
#include "pq-yao/garble_gates.h"
#include <iostream>

namespace emp {
template<typename T>
class GateGen: public CircuitExecution {
public:
    // Incremented after every gate
	uint64_t gid = 0;
	T * io;
    PRG prg;

	GateGen(T * io) :io(io) {};

    // Label for public bit 0 = All 0s
    // Label for public bit 1 = All 1s
	Label public_label(bool b) override {
		return b? one_label() : zero_label();
	}

    // Only the 0-th label is sufficient to determine if the input value is public
	bool is_public(const Label &b) {
		return isZero(&b) or isOne(&b);
	}

	void and_gate(Label& c0, Label& c1, const Label& a0, const Label& a1,
            const Label& b0, const Label& b1) override {
        // If one of the input labels is public, simply bitwise-AND the
        // input labels to get the output labels
		if (is_public(a0)) {
			c0.hi = _mm_and_si128(a0.hi, b0.hi);
			c0.lo = _mm_and_si128(a0.lo, b0.lo);
			c1.hi = _mm_and_si128(a0.hi, b1.hi);
			c1.lo = _mm_and_si128(a0.lo, b1.lo);
        } else if (is_public(b0)) {
			c0.hi = _mm_and_si128(a0.hi, b0.hi);
			c0.lo = _mm_and_si128(a0.lo, b0.lo);
			c1.hi = _mm_and_si128(a1.hi, b0.hi);
			c1.lo = _mm_and_si128(a1.lo, b0.lo);
        // Otherwise, generate a garbled table and send it to the evaluator
		} else {
            Label garbled_table[4];
			garble_gen_gate(c0, c1, a0, a1, b0, b1, gid++, garbled_table, &prg, AND);
			io->send_data(garbled_table, 8 * sizeof(block), true);
			return;
		}
	}

	void xor_gate(Label& c0, Label& c1, const Label& a0, const Label& a1,
            const Label& b0, const Label& b1) override {
        // If one of the input labels is 1, the output label is the NOT of the other input label
		if(isOne(&a0)) not_gate(c0, c1, b0, b1);
		else if (isOne(&b0)) not_gate(c0, c1, a0, a1);
        // If one of the input labels is 0, the output label is the the other input label
		else if (isZero(&a0)) c0 = b0, c1 = b1;
		else if (isZero(&b0)) c0 = a0, c1 = a1;
        // Otherwise, generate a garbled table and send it to the evaluator
		else {
            Label garbled_table[4];
			garble_gen_gate(c0, c1, a0, a1, b0, b1, gid++, garbled_table, &prg, XOR);
			io->send_data(garbled_table, 8 * sizeof(block), true);
		}
        return;
	}

	void not_gate(Label &b0, Label &b1, const Label &a0, const Label &a1) override {
        if (isZero(&a0)) b0 = one_label();
        else if (isOne(&a0)) b0 = zero_label();
        // If the input label is not public, garbler flips the mapping of the input labels
        else b0 = a1, b1 = a0;
        return;
	}
};
}
#endif// HALFGATE_GEN_H__
