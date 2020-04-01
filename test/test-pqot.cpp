#include "pq-ot/pq-ot.h"

using namespace std;
using namespace emp;

int role;
int port;
int num_threads = 1;
int num_ot = (1 << 17);
int bitlen = 256;
int plain_modulus_bitlen = 17;
string address = "127.0.0.1";

int main(int argc, char** argv){
	parse_party_and_port(argv, &role, &port);
    if (argc >= 4) address = argv[3];
    if (argc >= 5) num_ot = atoi(argv[4]);
    if (argc >= 6) bitlen = atoi(argv[5]);
    if (argc >= 7) num_threads = atoi(argv[6]);

    cout << "Performing " << num_ot << " 1oo2 OTs on " << bitlen
        << "-bit messages with " << num_threads << " threads" << endl;

    chrono::high_resolution_clock::time_point time_start, time_end;

    time_start = chrono::high_resolution_clock::now();
    NetIO* io = new NetIO(role == ALICE ? NULL : address.c_str(), port);
    PQOT ot(io, role, num_threads, plain_modulus_bitlen);
    time_end = chrono::high_resolution_clock::now();

    chrono::microseconds time_context = chrono::duration_cast<
    chrono::microseconds>(time_end - time_start);

    cout << "Context Initialization Time: " << time_context.count() << " microseconds" << endl;

    io->sync();

    uint64_t keygen_comm_start = io->get_total_comm();
    time_start = chrono::high_resolution_clock::now();

    // Keygen
    ot.keygen();

    time_end = chrono::high_resolution_clock::now();
    uint64_t keygen_comm_end = io->get_total_comm();
    uint64_t keygen_comm = keygen_comm_end - keygen_comm_start;

    chrono::microseconds time_keygen = chrono::duration_cast<
    chrono::microseconds>(time_end - time_start);

    cout << "KeyGen Time: " << time_keygen.count() << " microseconds" << endl;
    cout << "Keygen Comm: " << keygen_comm << " bytes" << endl;

    // Oblivious Transfer
    mpz_t *m_0, *m_1;
    gmp_randstate_t state;
    gmp_randinit_mt(state);
    bool* b;
    if (role == ALICE) {
        m_0 = new mpz_t[num_ot];
        m_1 = new mpz_t[num_ot];
        for(int i = 0; i < num_ot; i++){
            mpz_inits(m_0[i], m_1[i], NULL);
            mpz_urandomb(m_0[i], state, bitlen);
            mpz_urandomb(m_1[i], state, bitlen);
        }
    } else { // role == BOB
        m_0 = new mpz_t[num_ot];
        b = new bool[num_ot];
        for(int i = 0; i < num_ot; i++){
            mpz_init(m_0[i]);
            b[i] = rand() & 1;
        }
    }

    io->sync();

    uint64_t circuit_comm_start = io->get_total_comm();
    time_start = chrono::high_resolution_clock::now();

    if (role == ALICE) {
        ot.send_ot(m_0, m_1, num_ot, bitlen);
    } else { // role == BOB
        ot.recv_ot(m_0, b, num_ot, bitlen);
    }

    time_end = chrono::high_resolution_clock::now();
    uint64_t circuit_comm_end = io->get_total_comm();
    uint64_t circuit_comm = circuit_comm_end - circuit_comm_start;

    chrono::microseconds time_circuit = chrono::duration_cast<
    chrono::microseconds>(time_end - time_start);

    cout << "Circuit Time: " << time_circuit.count() << " microseconds" << endl;
    cout << "Circuit Comm: " << circuit_comm << " bytes" << endl;

    bool flag;
    if (role == ALICE) {
        flag = ot.verify(m_0, m_1, num_ot);
        for(int i = 0; i < num_ot; i++){
            mpz_clears(m_0[i], m_1[i], NULL);
        }
    } else { // role == BOB
        flag = ot.verify(m_0, b, num_ot);
        for(int i = 0; i < num_ot; i++){
            mpz_clear(m_0[i]);
        }
    }
    assert(flag == true && "Failed Operation");
    cout << "Successful Operation" << endl;

    return 0;
}
