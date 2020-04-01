#ifndef SEMIHONEST_EVA_H__
#define SEMIHONEST_EVA_H__
#include "emp-tool/emp-tool.h"
#include "pq-ot/pq-ot.h"
#include "pq-yao/gate-eva.h"

namespace emp {
class SemiHonestEva: public ProtocolExecution {
public:
	NetIO* io;
    PQOT *ot;
	GateEva<NetIO> * gc;
    bool batched_ot = false;
    int num_inputs;
    int counter = 0;
    bool* choice_bits;
    Label** labels;
	SemiHonestEva(NetIO *io, GateEva<NetIO> * gc, int num_inputs): ProtocolExecution(BOB) {
		this->io = io;
		this->gc = gc;	
        // Intialize the OT instance and exchange the public key
        ot = new PQOT(io, BOB);
        ot->keygen();
        // If num_inputs > 0, turn on the batched_ot mode,
        // where all the input OTs are done together, and
        // allocate enough space to store the input labels
        if(num_inputs > 0){
            this->num_inputs = num_inputs;
            batched_ot = true;
            choice_bits = new bool[num_inputs];
            labels = new Label*[num_inputs];
        };
	}
	~SemiHonestEva() {
		delete ot;
        delete[] labels;
        delete[] choice_bits;
	}

	void feed(Label* label0, Label* label1, int party, const bool* b, int length) {
        // If ALICE's (Garbler) input, receive the correct labels for b from garbler directly
		if(party == ALICE) {
            for(int i = 0; i < length; i++){
                io->recv_data(label0 + i, 2 * sizeof(block), true);
            }
        // Else, receive the correct labels for b using OT
		} else {
            // If batching input OTs, store the choice bits and the pointers
            // to the labels, and increment the counter. OTs will be done later
            // together and the labels will be updated then
            if(batched_ot) {
                memcpy(choice_bits + counter, b, length * sizeof(bool));
                for(int i = 0; i < length; i++){
                    labels[counter + i] = label0 + i;
                }
                counter += length;
            // Else perform OT right now to update the labels
            } else {
                mpz_t* temp = new mpz_t[length];
                bool* temp_b = new bool[length];
                memcpy(temp_b, b, length * sizeof(bool));
                for(int i = 0; i < length; i++) {
                    mpz_init(temp[i]);
                }
                ot->recv_ot(temp, temp_b, length, LABEL_BITLEN);
                for(int i = 0; i < length; i++) {
                    mpz_export(label0 + i, NULL, 1, sizeof(block), 0, 0, temp[i]);
                    mpz_clear(temp[i]);
                }
                delete[] temp;
            }
		}
	}

    // label is evaluator's label corresponding to b
	void reveal(bool * b, int party, const Label * label, int length) {
		for (int i = 0; i < length; ++i) {
            // If the label is public, no communication needed
			if(isOne(&label[i]))
				b[i] = true;
			else if (isZero(&label[i]))
				b[i] = false;
			else {
				bool lsb = getLSB(label[i].lo), tmp;
                // If the value is revealed to BOB or if it has to be made public,
                // receive the LSB (permutation-bit) of the 0-th label from garbler.
                // If it matches with the LSB of label, then b = 0, else b = 1
				if (party == BOB or party == PUBLIC) {
					io->recv_data(&tmp, 1, true);
					b[i] = (tmp != lsb);
                }
                // If the value is to be revealed to ALICE or if it has to be made public,
                // send the LSB (permutation-bit) of label to garbler.
				if (party == ALICE or party == PUBLIC) {
					io->send_data(&lsb, 1, true);
				}
			}
		}
	}

    void do_batched_ot() {
        assert(batched_ot == true);
        // Peform counter OTs together
        mpz_t* temp = new mpz_t[counter];
        for(int i = 0; i < counter; i++) {
            mpz_init(temp[i]);
        }
        ot->recv_ot(temp, choice_bits, counter, LABEL_BITLEN);
        for(int i = 0; i < counter; i++) {
            mpz_export(*(labels + i), NULL, 1, sizeof(block), 0, 0, temp[i]);
        }
        // Cleanup
        for(int i = 0; i < counter; i++) {
            mpz_clear(temp[i]);
        }
        delete[] temp;
        // Turn batched_ot mode off
        batched_ot = false;
    }
};
}

#endif// GARBLE_CIRCUIT_SEMIHONEST_H__
