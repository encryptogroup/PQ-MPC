#include "pq-ot/pq-otmain.h"

using namespace std;
using namespace seal;
using namespace seal::util;

// Divides iterations among threads such that each thread gets at most a block
// of ceil(num_iters/(num_threads*multiplier))*multiplier iterations and all
// blocks (except one) have iterations in a multiple of multiplier
void divide_iterations(vector<int> &iters_per_thread, int num_iters,
        int num_threads, int multiplier) {
    int num_chunks = floor((double) num_iters / multiplier);
    int num_chunks_per_thread = floor((double) num_chunks / num_threads);
    for(int i = 0; i < num_threads; i++) {
        iters_per_thread[i] = num_chunks_per_thread * multiplier;
    }
    int chunks_left = num_chunks - (num_chunks_per_thread) * num_threads;
    for(int i = 0; i < chunks_left; i++) { 
        iters_per_thread[i] += 1 * multiplier;
    }
    iters_per_thread[chunks_left] += (num_iters - num_chunks * multiplier);
}

// Sample a polynomial in ciphertext ring with uniformly random coefficients
// of bitlen bits
void sample_poly_coeffs_uniform(uint64_t *poly, uint32_t bitlen,
        shared_ptr<UniformRandomGenerator> random,
        shared_ptr<const SEALContext::ContextData> &context_data)
{
    assert(bitlen < 128 && bitlen > 0);
    auto &parms = context_data->parms();
    auto &coeff_modulus = parms.coeff_modulus();
    size_t coeff_count = parms.poly_modulus_degree();
    size_t coeff_mod_count = coeff_modulus.size();
    uint64_t bitlen_mask = (1ULL << (bitlen % 64)) - 1;

    RandomToStandardAdapter engine(random);
    for (size_t i = 0; i < coeff_count; i++) {
        if (bitlen < 64) {
            uint64_t noise = (uint64_t(engine()) << 32) | engine();
            noise &= bitlen_mask;
            for (size_t j = 0; j < coeff_mod_count; j++) {
                poly[i + (j * coeff_count)] = noise % coeff_modulus[j].value();
            }
        } else {
            uint64_t noise[2]; // LSB || MSB
            for(int j = 0; j < 2; j++){
                noise[0] = (uint64_t(engine()) << 32) | engine();
                noise[1] = (uint64_t(engine()) << 32) | engine();
            }
            noise[1] &= bitlen_mask;
            for (size_t j = 0; j < coeff_mod_count; j++) {
                poly[i + (j * coeff_count)] = barrett_reduce_128(noise, coeff_modulus[j]);
            }
        }
    }
}

// Flood the ciphertext ct with noise of noise_len bits
void flood_ciphertext(Ciphertext &ct,
        shared_ptr<const SEALContext::ContextData> &context_data,
        uint32_t noise_len, MemoryPoolHandle pool)
{
    auto &parms = context_data->parms();
    auto &coeff_modulus = parms.coeff_modulus();
    size_t coeff_count = parms.poly_modulus_degree();
    size_t coeff_mod_count = coeff_modulus.size();

    auto noise(allocate_poly(coeff_count, coeff_mod_count, pool));
    shared_ptr<UniformRandomGenerator> random(parms.random_generator()->create());

    // Flood the first coeff of ct
    sample_poly_coeffs_uniform(noise.get(), noise_len, random, context_data);
    for (size_t i = 0; i < coeff_mod_count; i++) {
        add_poly_poly_coeffmod(noise.get() + (i * coeff_count), 
            ct.data() + (i * coeff_count), coeff_count, 
            coeff_modulus[i], ct.data() + (i * coeff_count));
    }

    // Flood the second coeff of ct
    sample_poly_coeffs_uniform(noise.get(), noise_len, random, context_data);
    for (size_t i = 0; i < coeff_mod_count; i++) {
        add_poly_poly_coeffmod(noise.get() + (i * coeff_count), 
            ct.data(1) + (i * coeff_count), coeff_count, 
            coeff_modulus[i], ct.data(1) + (i * coeff_count));
    }
}

void SetupWorkerThread::run_sender() {
    // Receive public key from OT Receiver
    RecvTask task = recv->get_task(channel_id);
    stringstream ss;
    ss.write(task.data, task.length);

    PublicKey public_key;
    public_key.load(pkc->context, ss);

    pkc->encryptor = new Encryptor(pkc->context, public_key);
    pkc->evaluator = new Evaluator(pkc->context);
    pkc->batch_encoder = new BatchEncoder(pkc->context);

    // Also receive secret key from OT Receiver in the debug mode
#ifdef HE_DEBUG
    task = recv->get_task(channel_id);
    stringstream ss_sk;
    ss_sk.write(task.data, task.length);

    SecretKey secret_key;
    secret_key.load(pkc->context, ss_sk);

    pkc->decryptor = new Decryptor(pkc->context, secret_key);
#endif
}

void SetupWorkerThread::run_receiver() {
    KeyGenerator keygen(pkc->context);
    PublicKey public_key = keygen.public_key();
    SecretKey secret_key = keygen.secret_key();

    pkc->encryptor = new Encryptor(pkc->context, public_key);
    pkc->evaluator = new Evaluator(pkc->context);
    pkc->decryptor = new Decryptor(pkc->context, secret_key);
    pkc->batch_encoder = new BatchEncoder(pkc->context);

    // Send public key to OT Sender
    stringstream ss;
    public_key.save(ss);
    string str = ss.str();
    send->add_task(channel_id, str.size(), str.c_str());

    // Also send the secret key to OT Sender in the debug mode
#ifdef HE_DEBUG
    stringstream ss_sk;
    secret_key.save(ss_sk);
    string str_sk = ss_sk.str();
    send->add_task(channel_id, str_sk.size(), str_sk.c_str());
#endif
}

void ReceiverWorkerThread::run() {
    // Bitlength of OT messages to be embedded in slots
    int slot_bitlen = pkc->plain_modulus_bitlen - 1;
    int slots_per_msg = ceil((double) bitlen/slot_bitlen);
    int msgs_per_ctxt = floor((double) pkc->poly_degree/slots_per_msg);
    // Number of OT instances
    int num_iters = end_id - start_id;
    int num_cts = ceil((double) num_iters / msgs_per_ctxt) ;
    int slot_count = pkc->poly_degree;

    // Plaintext vectors for OT Receiver choice bits
    vector<vector<uint64_t>> pb(num_cts);

    for(int h = 0; h < num_cts; h++) {
        pb[h].resize(slot_count, 0);
        int offset = h * msgs_per_ctxt;
        for(int i = 0; i < msgs_per_ctxt && offset + i < num_iters; i++) {
            int index = start_id + offset + i;
            // Replicating the same choice bit in slots_per_msg slots
            for(int j = 0; j < slots_per_msg; j++) {
                pb[h][i*slots_per_msg + j] = (uint64_t) b[index];
            }
        }
    }

    vector<vector<uint64_t>> pm_b(num_cts);
    vector<Plaintext> ppb(num_cts);
    vector<Plaintext> ppm_b(num_cts);
    vector<Ciphertext> cb(num_cts);
    vector<Ciphertext> cm_b(num_cts);

    for(int h = 0; h < num_cts; h++) {
        pm_b[h].resize(slot_count);

        pkc->batch_encoder->encode(pb[h], ppb[h]);
        pkc->encryptor->encrypt(ppb[h], cb[h]);

        // Send cb (encryption of choice bit) to OT Sender
        stringstream ss;
        cb[h].save(ss);
        string str = ss.str();
        send->add_task(channel_id, str.size(), str.c_str());
    }

    for(int h = 0; h < num_cts; h++) {
        // Receive cm_b (encryption of message corresponding to choice bit) from OT Sender
        RecvTask task = recv->get_task(channel_id);
        stringstream ss;
        ss.write(task.data, task.length);
        cm_b[h].load(pkc->context, ss);
        pkc->decryptor->decrypt(cm_b[h], ppm_b[h]);
        pkc->batch_encoder->decode(ppm_b[h], pm_b[h]);
    }

    for(int h = 0; h < num_cts; h++){
        int offset = h * msgs_per_ctxt;
        for(int i = 0; i < msgs_per_ctxt && offset + i < num_iters; i++) {
            int index = start_id + offset + i;
            // Reconstruct the message m_b by concatenating
            // slots_per_msg integers of slot_bitlen-bits
            mpz_set_ui(m_b[index], pm_b[h][i * slots_per_msg]);
            for(int j = 1; j < slots_per_msg; j++){
                // Left shift by slot_bitlen bits
                mpz_mul_2exp(m_b[index], m_b[index], slot_bitlen);
                // Addition of message component to the lower slot_bitlen-bits of m_b
                mpz_add_ui(m_b[index], m_b[index], pm_b[h][i * slots_per_msg + j]);
            }
        }
    }
}

void SenderWorkerThread::run() {
    shared_ptr<UniformRandomGenerator> generator(FastPRNGFactory().create());
    // Bitlength of OT Sender messages to be embedded in slots
    int slot_bitlen = pkc->plain_modulus_bitlen - 1;
    int slots_per_msg = ceil((double) bitlen/slot_bitlen);
    int msgs_per_ctxt = floor((double) pkc->poly_degree/slots_per_msg);
    // Number of OT instances
    int num_iters = end_id - start_id;
    int num_cts = ceil((double) num_iters / msgs_per_ctxt) ;
    int slot_count = pkc->poly_degree;

    // Plaintext vector with 1 in all slots
    vector<uint64_t> p_1(slot_count);
    for(int i = 0; i < slot_count; i++){
        p_1[i] = 1ULL;
    }

    // Plaintext vectors for OT Sender messages
    vector<vector<uint64_t>> pm_0(num_cts), pm_1(num_cts);

    mpz_t temp_0, temp_1;
    // mask is used to extract the lower slot_bitlen-bits from an integer
    mpz_t mask;
    // slice holds the slot_bitlen-bits long components of OT Sender messages
    // extracted using masking
    mpz_t slice;
    mpz_inits(temp_0, temp_1, slice, mask, NULL);
    // mask = (1 << slot_bitlen) - 1;
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, slot_bitlen);
    mpz_sub_ui(mask, mask, 1);
    for(int h = 0; h < num_cts; h++) {
        pm_0[h].resize(slot_count);
        pm_1[h].resize(slot_count);
        int offset = h * msgs_per_ctxt;
        for(int i = 0; i < msgs_per_ctxt && offset + i < num_iters; i++) {
            int index = start_id + offset + i;
            mpz_set(temp_0, m_0[index]);
            mpz_set(temp_1, m_1[index]);
            for(int j = slots_per_msg - 1; j >= 0; j--) {
                // Extraction of lower slot_bitlen-bits of OT Sender messages
                mpz_and(slice, temp_0, mask);
                pm_0[h][i*slots_per_msg + j] = mpz_get_ui(slice);
                mpz_and(slice, temp_1, mask);
                pm_1[h][i*slots_per_msg + j] = mpz_get_ui(slice);
                // Right shift by slot_bitlen bits
                mpz_fdiv_q_2exp(temp_0, temp_0, slot_bitlen);
                mpz_fdiv_q_2exp(temp_1, temp_1, slot_bitlen);
            }
        }
    }
    mpz_clears(temp_0, temp_1, slice, mask, NULL);

    // Plaintexts for OT Sender messages
    vector<Plaintext> ppm_0(num_cts), ppm_1(num_cts);
    // Plaintext with 1 in all slots
    Plaintext pp_1;
    vector<Ciphertext> cb(num_cts), cm_b(num_cts);

    for(int h = 0; h < num_cts; h++){
        pkc->batch_encoder->encode(pm_0[h], ppm_0[h]);
        pkc->batch_encoder->encode(pm_1[h], ppm_1[h]);
        pkc->batch_encoder->encode(p_1, pp_1);

        // Receive cb (encryption of choice bit) from OT Receiver
        RecvTask task = recv->get_task(channel_id);
        stringstream ss;
        ss.write(task.data, task.length);
        cb[h].load(pkc->context, ss);
    }

    for(int h = 0; h < num_cts; h++){
#ifdef HE_DEBUG
        if (!h) {
            cout << "Noise budget of fresh encryption: "
                << pkc->decryptor->invariant_noise_budget(cb[h]) << " bits" << endl;
        }
#endif

        // cm_b = m_0 * (1 - cb) + m_1 * (cb)
        pkc->evaluator->negate(cb[h], cm_b[h]);
        pkc->evaluator->add_plain_inplace(cm_b[h], pp_1);
        pkc->evaluator->multiply_plain_inplace(cm_b[h], ppm_0[h]);
        pkc->evaluator->multiply_plain_inplace(cb[h], ppm_1[h]);
        pkc->evaluator->add_inplace(cm_b[h], cb[h]);
#ifdef HE_DEBUG
        if (!h) {
            cout << "Noise budget after OT computation: "
                << pkc->decryptor->invariant_noise_budget(cm_b[h]) << " bits" << endl;
        }
#endif

        // Switching to a smaller ciphertext modulus for efficiency
        if (pkc->plain_modulus_bitlen == 33) {
            pkc->evaluator->mod_switch_to_next_inplace(cm_b[h]);
#ifdef HE_DEBUG
            if (!h) {
                cout << "Noise budget after mod-switch: "
                    << pkc->decryptor->invariant_noise_budget(cm_b[h]) << " bits" << endl;
            }
#endif
        }

        // Noise Flooding required for circuit privacy
        parms_id_type parms_id = cm_b[h].parms_id();
        shared_ptr<const SEALContext::ContextData> context_data_
            = pkc->context->context_data(parms_id);
        // Noise bitlengths determined heuristically to guarantee
        // statistical security of at least 40 bits
        flood_ciphertext(cm_b[h], context_data_, 89 - pkc->plain_modulus_bitlen);
#ifdef HE_DEBUG
        if (!h) {
            cout << "Noise budget after noise flooding: "
                << pkc->decryptor->invariant_noise_budget(cm_b[h]) << " bits" << endl;
        }
#endif

        // Switching to a smaller ciphertext modulus for efficiency
        pkc->evaluator->mod_switch_to_next_inplace(cm_b[h]);
#ifdef HE_DEBUG
        if (!h) {
            cout << "Noise budget after mod-switch: "
                << pkc->decryptor->invariant_noise_budget(cm_b[h]) << " bits" << endl;
        }
#endif
        // Send cm_b (encryption of message corresponding to choice bit) to OT Receiver
        stringstream ss;
        cm_b[h].save(ss);
        string str = ss.str();
        send->add_task(channel_id, str.size(), str.c_str());
    }
}
