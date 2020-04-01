#include "pq-yao/emp-sh2pc.h"

using namespace emp;
using namespace std;

const string circuit_file_location = "../../emp-tool/circuits/files";
int port, party;
string file = circuit_file_location;
int n_inputs, n_outputs;
int num_iter = 100;
string circuit = "aes";
CircuitFile* cf;
NetIO* io;
double time_send_input, time_ot_input, time_circuit, time_input, time_total;
uint64_t comm_send_input, comm_ot_input, comm_circuit, comm_input, comm_total;

int map_case(string x){
    if (x == "aes")
        return 0;
    else if (x == "add")
        return 1;
    else if (x == "mult")
        return 2;
    else
        return -1;
}

void test() {
    io->sync();
    uint64_t comm_start = io->get_total_comm();
	auto time_start = clock_start();
	Integer a(n_inputs * num_iter, 16807, ALICE);
	Integer b(n_inputs * num_iter, 282475249, BOB);
	Integer c(n_outputs * num_iter, 0, PUBLIC);
    time_send_input = time_from(time_start);
    comm_send_input = io->get_total_comm() - comm_start;

    io->sync();
    comm_start = io->get_total_comm();
    time_start = clock_start();
    ProtocolExecution::prot_exec->do_batched_ot();
    time_ot_input = time_from(time_start);
    comm_ot_input = io->get_total_comm() - comm_start;

    time_input = time_send_input + time_ot_input;
    comm_input = comm_send_input + comm_ot_input;
    cout << "Time Input: " << time_input << endl;
    cout << "Comm Input: " << comm_input << endl;

    io->sync();
    comm_start = io->get_total_comm();
    time_start = clock_start();
	for(int i = 0; i < num_iter; ++i) {
        cf->compute(c.bits, a.bits, b.bits);
	}
    time_circuit = time_from(time_start);
    comm_circuit = io->get_total_comm() - comm_start;
    cout << "Time Circuit: " << time_circuit << endl;
    cout << "Comm Circuit: " << comm_circuit << endl;

    string output = c.reveal<string>(PUBLIC);
    time_total = time_input + time_circuit;
    comm_total = comm_input + comm_circuit;
	cout << "Time Total: " << time_total << endl;
    cout << "Party: " << party << "; Output: "<< output << endl;

    switch(map_case(circuit)){
        case 0:
            assert(output == "27188201084259728602474194279627260317" && "Failed Operation");
            break;
        case 1:
            assert(output == "282492056" && "Failed Operation");
            break;
        case 2:
            assert(output == "4747561509943" && "Failed Operation");
            break;
        default:
            throw std::invalid_argument("Circuit Not implemented");
            break;
    }
    cout << "Successful Operation" << endl;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	io = new NetIO(party==ALICE?nullptr:"127.0.0.1", port);

    if (argc >= 4) circuit = argv[3];
    if (argc >= 5) num_iter = atoi(argv[4]);

    switch(map_case(circuit)){
        case 0:
            file = file + "/AES-non-expanded.txt";
            n_inputs = 128;
            n_outputs = 128;
            break;
        case 1:
            file = file + "/adder_32bit.txt";
            n_inputs = 32;
            n_outputs = 33;
            break;
        case 2:
            file = file + "/mult_32bit.txt";
            n_inputs = 32;
            n_outputs = 64;
            break;
        default:
            throw std::invalid_argument("Circuit Not implemented");
            break;
    }
    cout << "Performing " << num_iter << " runs of " << circuit << " circuit on "
        << n_inputs << "-bit inputs and " << n_outputs << "-bit outputs" << endl;

    cf = new CircuitFile(file.c_str());
	setup_semi_honest(io, party, n_inputs * num_iter);
	test();

	delete io;
}
