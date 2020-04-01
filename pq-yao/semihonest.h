#ifndef SEMIHONEST_H__
#define SEMIHONEST_H__
#include "pq-yao/semihonest-gen.h"
#include "pq-yao/semihonest-eva.h"

namespace emp {
inline void setup_semi_honest(NetIO* io, int party, int num_inputs = 0) {
	if(party == ALICE) {
		GateGen<NetIO> * t = new GateGen<NetIO>(io);
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new SemiHonestGen(io, t, num_inputs);
	} else {
		GateEva<NetIO> * t = new GateEva<NetIO>(io);
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new SemiHonestEva(io, t, num_inputs);
	}
}
}
#endif
