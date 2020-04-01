#ifndef CIRCUIT_EXECUTION_H__
#define CIRCUIT_EXECUTION_H__
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
namespace emp {
class CircuitExecution { 
public:
#ifndef THREADING
	static CircuitExecution * circ_exec;
#else
	static __thread CircuitExecution * circ_exec;
#endif
	virtual void and_gate(Label& c0, Label& c1, const Label& a0, const Label& a1, const Label& b0, const Label& b1) {}
	virtual void xor_gate(Label& c0, Label& c1, const Label& a0, const Label& a1, const Label& b0, const Label& b1) {}
	virtual void not_gate(Label& b0, Label& b1, const Label& a0, const Label& a1) {}
	virtual Label public_label(bool b) {return Label(zero_,zero_);}
	virtual ~CircuitExecution (){ }
};
enum RTCktOpt{on, off};
}
#endif
