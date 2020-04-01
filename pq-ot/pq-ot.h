#ifndef PQ_OT_H__
#define PQ_OT_H__
#include <chrono>
#include <cassert>
#include <fstream>
#include "pq-ot/pq-otmain.h"

class PQOT{
public:
    PQOT(emp::NetIO* io, int role, int num_threads = 1, int plain_modulus_bitlen = 17){
        assert(role == 1 || role == 2);
        // HE Parameters configured only for the following two choices
        assert(plain_modulus_bitlen == 17 || plain_modulus_bitlen == 33);

        this->role = role;
        this->plain_modulus_bitlen = plain_modulus_bitlen;

        int poly_degree;
        if (plain_modulus_bitlen == 17) poly_degree = 8192;
        else poly_degree = 16384; // plain_modulus_bitlen == 33

        // Setup the parameters and the context required by the HE scheme
        pkc = new Cryptosystem(this->role, plain_modulus_bitlen, poly_degree);

        this->num_threads = num_threads;
        this->io = io;
        // Dedicated sender IO thread
        this->send = new SendThread(io);
        // Dedicated receiver IO thread
        this->recv = new RecvThread(io, num_threads);
    }

    ~PQOT() {
        // Stop the IO threads
        send->signal_end();
        send->wait();
        recv->wait();
    }

    void keygen();
    void send_ot(mpz_t* m_0, mpz_t* m_1, int num_ot, int bitlen, bool verify_ot = false);
    void recv_ot(mpz_t* m_b, bool* b, int num_ot, int bitlen, bool verify_ot = false);
    bool verify(mpz_t* m_0, mpz_t* m_1, int num_ot);
    bool verify(mpz_t* m_b, bool* b, int num_ot);

    int role;
    int plain_modulus_bitlen;
    int num_threads;
private:
    emp::NetIO* io;
    SendThread* send;
    RecvThread* recv;
    Cryptosystem* pkc;
};
#endif //RLWE_OT_H__
