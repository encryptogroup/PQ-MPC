#ifndef PROTOCOL_EXECUTION_H__
#define PROTOCOL_EXECUTION_H__
#include <pthread.h>
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"

namespace emp {
class ProtocolExecution { 
public:
	int cur_party;
	static ProtocolExecution * prot_exec;

	ProtocolExecution(int party = PUBLIC) : cur_party (party) {}
	virtual ~ProtocolExecution() {}
	virtual void feed(Label * lbls, int party, const bool* b, int nel) {}
	virtual void feed(Label * lbls0, Label * lbls1, int party, const bool* b, int nel) {}
	virtual void reveal(bool*out, int party, const Label *lbls, int nel) {}
    virtual void do_batched_ot() {}
	virtual void finalize() {}
};
}
#endif
