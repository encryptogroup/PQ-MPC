#ifndef SEMIHONEST_GEN_H__
#define SEMIHONEST_GEN_H__
#include "emp-tool/emp-tool.h"
#include "pq-ot/pq-ot.h"
#include "pq-yao/gate_gen.h"
#include <iostream>

namespace emp {
class SemiHonestGen: public ProtocolExecution {
public:
	NetIO* io;
    PQOT *ot;
	PRG prg;
	GateGen<NetIO> * gc;
    bool batched_ot = false;
    int num_inputs;
    Label *labels0, *labels1;
    int counter = 0;
	SemiHonestGen(NetIO* io, GateGen<NetIO>* gc, int num_inputs): ProtocolExecution(ALICE) {
		this->io = io;
		this->gc = gc;	
        // Intialize the OT instance and exchange the public key
        ot = new PQOT(io, ALICE);
        ot->keygen();
        // If num_inputs > 0, turn on the batched_ot mode,
        // where all the input OTs are done together, and
        // allocate enough space to store the input labels
        if(num_inputs > 0){
            this->num_inputs = num_inputs;
            batched_ot = true;
            labels0 = new Label[num_inputs];
            labels1 = new Label[num_inputs];
        };
	}
	~SemiHonestGen() {
		delete ot;
        delete[] labels0;
        delete[] labels1;
	}

	void feed(Label* label0, Label* label1, int party, const bool* b, int length) {
        // Sample random labels for the input b
        prg.random_label(label0, length);
        prg.random_label(label1, length);
        for(int i = 0; i < length; i++){
            // Permutation bit of label0
            bool s0 = getLSB(label0[i].lo);
            // Set LSB of label1 as 1 ^ s0
            if (s0) set_lsb_zero(label1[i].lo);
            else set_lsb_one(label1[i].lo);
        }
        // If ALICE's (Garbler) input, send the correct label for b to evaluator directly
		if(party == ALICE) {
            for(int i = 0; i < length; i++){
                if (b[i]) io->send_data(label1 + i, 2 * sizeof(block), true);
                else io->send_data(label0 + i, 2 * sizeof(block), true);
            }
        // Else, perform OT to send the correct labels for b
		} else {
            // If batching input OTs, store the labels and increment the counter.
            // OTs will be done later together
            if(batched_ot) {
                memcpy(labels0 + counter, label0, length * sizeof(Label));
                memcpy(labels1 + counter, label1, length * sizeof(Label));
                counter += length;
            // Else perform OT right now to send the labels
            } else {
                mpz_t* temp0 = new mpz_t[length];
                mpz_t* temp1 = new mpz_t[length];
                for(int i = 0; i < length; i++) {
                    mpz_inits(temp0[i], temp1[i], NULL);
                    mpz_import(temp0[i], 2, 1, sizeof(block), 0, 0, label0 + i);
                    mpz_import(temp1[i], 2, 1, sizeof(block), 0, 0, label1 + i);
                }
                ot->send_ot(temp0, temp1, length, LABEL_BITLEN);
                for(int i = 0; i < length; i++) {
                    mpz_clears(temp0[i], temp1[i], NULL);
                }
                delete[] temp0;
                delete[] temp1;
            }
		}
	}

    // label is the 0-th label of garbler corresponding to b
	void reveal(bool* b, int party, const Label * label, int length) {
		for (int i = 0; i < length; ++i) {
            // If the label is public, no communication needed
			if(isOne(&label[i]))
				b[i] = true;
			else if (isZero(&label[i]))
				b[i] = false;
			else {
				bool lsb = getLSB(label[i].lo), tmp;
                // If the value is to be revealed to BOB or if it has to be made public,
                // send the LSB (permutation-bit) of label to evaluator.
				if (party == BOB or party == PUBLIC) {
					io->send_data(&lsb, 1, true);
				}
                // If the value is revealed to ALICE or if it has to be made public,
                // receive the LSB (permutation-bit) of the evaluator's label.
                // If it matches with the LSB of label, then b = 0, else b = 1
                if(party == ALICE || party == PUBLIC) {
					io->recv_data(&tmp, 1, true);
					b[i] = (tmp != lsb);
				}
			}
		}
	}

    void do_batched_ot() {
        assert(batched_ot == true);
        // Peform counter OTs together
        mpz_t* temp0 = new mpz_t[counter];
        mpz_t* temp1 = new mpz_t[counter];
        for(int i = 0; i < counter; i++) {
            mpz_inits(temp0[i], temp1[i], NULL);
            mpz_import(temp0[i], 2, 1, sizeof(block), 0, 0, labels0 + i);
            mpz_import(temp1[i], 2, 1, sizeof(block), 0, 0, labels1 + i);
        }
        ot->send_ot(temp0, temp1, counter, LABEL_BITLEN);
        // Cleanup
        for (int i = 0; i < counter; i++) {
            mpz_clears(temp0[i], temp1[i], NULL);
        }
        delete[] temp0;
        delete[] temp1;
        // Turn batched_ot mode off
        batched_ot = false;
    }
};
}
#endif //SEMIHONEST_GEN_H__
