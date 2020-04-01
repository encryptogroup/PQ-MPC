#ifndef GATE_EVA_H__
#define GATE_EVA_H__
#include "emp-tool/io/net-io.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/utils.h"
#include "emp-tool/execution/circuit_execution.h"
#include "pq-yao/garble_gates.h"
#include <iostream>

namespace emp {
template<typename T>
class GateEva: public CircuitExecution{
public:
    // Incremented after every gate
	uint64_t gid = 0;
	T * io;

	GateEva(T * io) :io(io) {};

    // Label for bit value 0 = All 0s
    // Label for bit value 1 = All 1s
	Label public_label(bool b) override {
		return b? one_label() : zero_label();
	}

	bool is_public(const Label &b) {
		return isZero(&b) or isOne(&b);
	}

	void and_gate(Label& c0, Label& c1, const Label& a0, const Label& a1,
            const Label& b0, const Label& b1) override {
        // If one of the input labels is public, simply bitwise-AND the
        // input labels to get the output label
		if (is_public(a0) or is_public(b0)) {
			c0.hi = _mm_and_si128(a0.hi, b0.hi);
			c0.lo = _mm_and_si128(a0.lo, b0.lo);
        // Otherwise, receive the garbled table and decrypt it to get the output label
		} else {
            Label garbled_table[4];
			io->recv_data(garbled_table, 8 * sizeof(block), true);
            garble_eval_gate(c0, a0, b0, gid++, garbled_table);
		}
        return;
	}

	void xor_gate(Label& c0, Label& c1, const Label& a0, const Label& a1,
            const Label& b0, const Label& b1) override {
        // If one of the input labels is 1, the output label is the NOT of the other input label
		if(isOne(&a0)) not_gate(c0, c1, b0, b1);
		else if (isOne(&b0)) not_gate(c0, c1, a0, a1);
        // If one of the input labels is 0, the output label is the the other input label
		else if (isZero(&a0)) c0 = b0;
		else if (isZero(&b0)) c0 = a0;
        // Otherwise, receive the garbled table and decrypt it to get the output label
		else {
            Label garbled_table[4];
			io->recv_data(garbled_table, 8 * sizeof(block), true);
            garble_eval_gate(c0, a0, b0, gid++, garbled_table);
		}
        return;
	}
	void not_gate(Label &b0, Label &b1, const Label &a0, const Label &a1) override {
        if (isZero(&a0)) b0 = one_label();
        else if (isOne(&a0)) b0 = zero_label();
        // If the input label is not public, evaluator does nothing
        else b0 = a0;
        return;
	}
};
}
#endif// HALFGATE_EVA_H__
