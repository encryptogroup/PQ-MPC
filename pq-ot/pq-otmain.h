#ifndef PQ_OT_MAIN_H__
#define PQ_OT_MAIN_H__
#include "pq-ot/io-thread.h"
#include "seal/seal.h"
#include "seal/randomgen.h"
#include "seal/encryptor.h"
#include "seal/util/polyarithsmallmod.h"
#include "seal/util/randomtostd.h"
#include "gmp.h"
#include "gmpxx.h"

// #define HE_DEBUG

void divide_iterations(std::vector<int> &iters_per_thread, int num_iters,
        int num_threads, int multiplier = 1);
void sample_poly_coeffs_uniform(uint64_t *poly, uint32_t bitlen,
        std::shared_ptr<seal::UniformRandomGenerator> random,
        std::shared_ptr<const seal::SEALContext::ContextData> &context_data);
void flood_ciphertext(seal::Ciphertext &ct,
        std::shared_ptr<const seal::SEALContext::ContextData> &context_data,
        uint32_t noise_len, seal::MemoryPoolHandle pool = seal::MemoryManager::GetPool());

class Cryptosystem {
public:
    Cryptosystem (int role, int plain_modulus_bitlen, int poly_degree) {
        this->poly_degree = poly_degree;
        this->plain_modulus_bitlen = plain_modulus_bitlen;

        parms = new seal::EncryptionParameters(seal::scheme_type::BFV);
        parms->set_poly_modulus_degree(poly_degree);
        switch (plain_modulus_bitlen) {
            case 17: {
                plain_modulus = 65537; // 17-bit prime
                std::vector<seal::SmallModulus> q;
                // 100-bit ciphertext modulus
                q.push_back(seal::small_mods_60bit(0));
                q.push_back(seal::small_mods_40bit(0));
                parms->set_coeff_modulus(q);
                break;
            }
            case 33: {
                plain_modulus = 7400521729; // 33-bit prime
                std::vector<seal::SmallModulus> q;
                // 150-bit ciphertext modulus
                q.push_back(seal::small_mods_50bit(0));
                q.push_back(seal::small_mods_50bit(1));
                q.push_back(seal::small_mods_50bit(2));
                parms->set_coeff_modulus(q);
                break;
            }
        }
        parms->set_plain_modulus(plain_modulus);
        context = seal::SEALContext::Create(*parms);
    }

    seal::EncryptionParameters* parms;
    std::shared_ptr<seal::SEALContext> context;
    seal::Encryptor* encryptor;
    seal::Evaluator* evaluator;
    seal::BatchEncoder* batch_encoder;
    seal::Decryptor* decryptor;
    uint64_t plain_modulus;
    int poly_degree;
    int plain_modulus_bitlen;
};

class SetupWorkerThread : public BaseThread {
public:
    SetupWorkerThread(int role, int channel_id, Cryptosystem* pkc, SendThread* send, RecvThread* recv) {
        this->role = role;
        this->channel_id = channel_id;
        this->pkc = pkc;
        this->send = send;
        this->recv = recv;
    }

    void run_sender();
    void run_receiver();

    void run() {
        switch (role) {
            case emp::ALICE:
                run_sender();
                break;
            case emp::BOB:
                run_receiver();
                break;
            default:
                break;
        }
    }

private:
    int role;
    int channel_id;
    Cryptosystem* pkc;
    SendThread* send;
    RecvThread* recv;
};

class SenderWorkerThread : public BaseThread {
public:
    SenderWorkerThread(int channel_id, int bitlen, Cryptosystem* pkc, SendThread* send, RecvThread* recv) {
        this->channel_id = channel_id;
        this->bitlen = bitlen;
        this->pkc = pkc;
        this->send = send;
        this->recv = recv;
    }

    void set_iteration_bounds(int start_id, int end_id) {
        this->start_id = start_id;
        this->end_id = end_id;
    }

    void set_input(mpz_t* m_0, mpz_t* m_1) {
        this->m_0 = m_0;
        this->m_1 = m_1;
    }

    void run();
private:
    int channel_id;
    int start_id, end_id;
    int bitlen;
    Cryptosystem* pkc;
    SendThread* send;
    RecvThread* recv;
    mpz_t *m_0, *m_1;
};

class ReceiverWorkerThread : public BaseThread {
public:
    ReceiverWorkerThread(int channel_id, int bitlen, Cryptosystem* pkc, SendThread* send, RecvThread* recv) {
        this->channel_id = channel_id;
        this->bitlen = bitlen;
        this->pkc = pkc;
        this->send = send;
        this->recv = recv;
    }

    void set_iteration_bounds(int start_id, int end_id) {
        this->start_id = start_id;
        this->end_id = end_id;
    }

    void set_input(bool* b) {
        this->b = b;
    }

    void set_output(mpz_t* m_b) {
        this->m_b = m_b;
    }

    void run();
private:
    int channel_id;
    int start_id, end_id;
    int bitlen;
    Cryptosystem* pkc;
    SendThread* send;
    RecvThread* recv;
    mpz_t* m_b;
    bool* b;
};
#endif //RLWE_OT_MAIN_H__
