#include "pq-ot/pq-ot.h"

using namespace std;

// Utility functions to save integers to a stream, and load integers from a stream
void save(uint64_t* input, int length, ostream& stream){
    for(int i = 0; i < length; i++)
        stream << input[i] << " ";
    stream << endl;
}

void load(uint64_t* output, int length, istream& stream){
    for(int i = 0; i < length; i++)
        stream >> output[i];
    return;
}

void save(mpz_t *input, int length, ostream& stream){
    for(int i = 0; i < length; i++)
        stream << input[i] << " ";
    stream << endl;
}

void load(mpz_t *output, int length, istream& stream){
    for(int i = 0; i < length; i++)
        stream >> output[i];
    return;
}

// OT Receiver generates a key pair and sends the public key to OT Sender
void PQOT::keygen(){
    // Start the IO threads for key exchange
    switch (role) {
        case 1:
            recv->start();
            break;
        case 2:
            send->start();
            break;
    }

    SetupWorkerThread* setup_worker;
    // Use channel 0 for the exchange
    setup_worker = new SetupWorkerThread(role, 0, pkc, send, recv);
    setup_worker->start();
    setup_worker->wait();

    // Stop the IO threads
    switch (role) {
        case 1:
            recv->wait();
            break;
        case 2:
            send->signal_end();
            send->wait();
    }
}

void PQOT::send_ot(mpz_t* m_0, mpz_t* m_1, int num_ot, int bitlen, bool verify_ot){
    // Start the IO threads
    send->start();
    recv->start();

    int id = 0, count = 0;
    vector<int> ot_per_thread(num_threads);
    int slot_bitlen = pkc->plain_modulus_bitlen - 1;
    int slots_per_msg = ceil((double) bitlen/slot_bitlen);
    int msgs_per_ctxt = floor((double) pkc->poly_degree/slots_per_msg);
    // Divide num_ot among threads
    divide_iterations(ot_per_thread, num_ot, num_threads, msgs_per_ctxt);
    vector<SenderWorkerThread*> sender_workers(num_threads);
    for(int i = 0; i < num_threads; i++) {
        if(ot_per_thread[i] > 0) {
            // Intialize the OT Sender thread
            sender_workers[i] = new SenderWorkerThread(i, bitlen, pkc, send, recv);
            // Set the range of OT indices it is supposed to compute
            sender_workers[i]->set_iteration_bounds(id, id + ot_per_thread[i]);
            sender_workers[i]->set_input(m_0, m_1);
            id += ot_per_thread[i];
            // Start the OT Sender thread
            sender_workers[i]->start();
            count++;
        }
    }

    for(int i = 0; i < count; i++) {
        sender_workers[i]->wait();
    }

    // Stop the IO threads
    send->signal_end();
    send->wait();
    recv->wait();
    // Verify the computed OTs
    if (verify_ot) verify(m_0, m_1, num_ot);
}

void PQOT::recv_ot(mpz_t* m_b, bool* b, int num_ot, int bitlen, bool verify_ot){
    // Start the IO threads
    send->start();
    recv->start();

    int id = 0, count = 0;
    vector<int> ot_per_thread(num_threads);
    int slot_bitlen = pkc->plain_modulus_bitlen - 1;
    int slots_per_msg = ceil((double) bitlen/slot_bitlen);
    int msgs_per_ctxt = floor((double) pkc->poly_degree/slots_per_msg);
    // Divide num_ot among threads
    divide_iterations(ot_per_thread, num_ot, num_threads, msgs_per_ctxt);
    vector<ReceiverWorkerThread*> receiver_workers(num_threads);
    for(int i = 0; i < num_threads; i++) {
        if(ot_per_thread[i] > 0) {
            // Intialize the OT Receiver thread
            receiver_workers[i] = new ReceiverWorkerThread(i, bitlen, pkc, send, recv);
            // Set the range of OT indices it is supposed to compute
            receiver_workers[i]->set_iteration_bounds(id, id + ot_per_thread[i]);
            receiver_workers[i]->set_input(b);
            receiver_workers[i]->set_output(m_b);
            id += ot_per_thread[i];
            // Start the OT Receiver thread
            receiver_workers[i]->start();
            count++;
        }
    }

    for(int i = 0; i < count; i++) {
        receiver_workers[i]->wait();
    }

    // Stop the IO threads
    send->signal_end();
    send->wait();
    recv->wait();
    // Verify the computed OTs
    if (verify_ot) verify(m_b, b, num_ot);
}

bool PQOT::verify(mpz_t* m_0, mpz_t* m_1, int num_ot){
    // Start sender IO thread
    send->start();
    stringstream ss;
    save(m_0, num_ot, ss);
    save(m_1, num_ot, ss);

    // Send the OT Sender messages to OT Receiver
    string str = ss.str();
    send->add_task(0, str.size(), str.c_str());

    // Stop sender IO thread
    send->signal_end();
    send->wait();
    return true;
}

bool PQOT::verify(mpz_t* m_b, bool* b, int num_ot){
    // Start receiver IO thread
    recv->start();

    bool flag = true;

    mpz_t *m_0, *m_1;
    m_0 = new mpz_t[num_ot];
    m_1 = new mpz_t[num_ot];
    for(int i = 0; i < num_ot; i++){
        mpz_inits(m_0[i], m_1[i], NULL);
    }

    // Receive the OT Sender messages from OT Sender
    RecvTask task = recv->get_task(0);
    stringstream ss;
    ss.write(task.data, task.length);

    load(m_0, num_ot, ss);
    load(m_1, num_ot, ss);

    // Check if all the OTs are computed correctly
    for(int h = 0; h < num_ot; h++) {
        if((b[h] == 0 && (mpz_cmp(m_0[h], m_b[h]) != 0))
           || (b[h] == 1 && (mpz_cmp(m_1[h], m_b[h]) != 0))){
            flag = false;
        }
    }
    for(int i = 0; i < num_ot; i++){
        mpz_clears(m_0[i], m_1[i], NULL);
    }
    delete[] m_0;
    delete[] m_1;

    // Stop receiver IO thread
    recv->wait();
    return flag;
}
