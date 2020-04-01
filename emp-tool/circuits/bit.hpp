inline Bit::Bit(bool b, int party) {
	if (party == PUBLIC)
		bit0 = CircuitExecution::circ_exec->public_label(b);
	else ProtocolExecution::prot_exec->feed(&bit0, &bit1, party, &b, 1);
}

inline Bit Bit::select(const Bit & select, const Bit & new_v) const{
	Bit tmp = *this;
	tmp = tmp ^ new_v;
	tmp = tmp & select;
	return *this ^ tmp;
}

template<typename O>
inline O Bit::reveal(int party) const {
	O res;
	ProtocolExecution::prot_exec->reveal(&res, party, &bit0, 1);
	return res;
}

template<>
inline string Bit::reveal<string>(int party) const {
	bool res;
	ProtocolExecution::prot_exec->reveal(&res, party, &bit0, 1);
	return res ? "true" : "false";
}

inline Bit Bit::operator==(const Bit& rhs) const {
	return !(*this ^ rhs);
}

inline Bit Bit::operator!=(const Bit& rhs) const {
	return (*this) ^ rhs;
}

inline Bit Bit::operator &(const Bit& rhs) const{
	Bit res;
	CircuitExecution::circ_exec->and_gate(res.bit0, res.bit1, bit0, bit1, rhs.bit0, rhs.bit1);
	return res;
}
inline Bit Bit::operator ^(const Bit& rhs) const{
	Bit res;
	CircuitExecution::circ_exec->xor_gate(res.bit0, res.bit1, bit0, bit1, rhs.bit0, rhs.bit1);
	return res;
}

inline Bit Bit::operator |(const Bit& rhs) const{
	return (*this ^ rhs) ^ (*this & rhs);
}

inline Bit Bit::operator!() const {
    Bit res;
    CircuitExecution::circ_exec->not_gate(res.bit0, res.bit1, bit0, bit1);
	return res;
}
