#ifndef PQ_OT_IO_THREAD_H__
#define PQ_OT_IO_THREAD_H__
#include <emp-tool/emp-tool.h>
#include <pthread.h>
#include <queue>
#include <mutex>
#include <condition_variable>

#define ADMIN_CHANNEL 255

// ConcurrentQueue implementation taken from
// https://juanchopanzacpp.wordpress.com/2013/02/26/concurrent-queue-c11/
template <typename T>
class ConcurrentQueue
{
public:
    T pop() {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty()) {
            cond_.wait(mlock);
        }
        auto val = queue_.front();
        queue_.pop();
        return val;
    }

    void pop(T& item) {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty()) {
            cond_.wait(mlock);
        }
        item = queue_.front();
        queue_.pop();
    }

    void push(const T& item) {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
    }

    void flush() {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        mlock.unlock();
    }

    ConcurrentQueue()=default;
    ConcurrentQueue(const ConcurrentQueue&) = delete;            // disable copying
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete; // disable assignment

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class BaseThread {
public:
    BaseThread() {
        running = false;
        thread_id = 0;
    }
    virtual ~BaseThread() {};

public:
    bool start() {
        running = !pthread_create(&thread_id, NULL, run_handler, (void*) this);
        return running;
    }

    bool wait() {
        if (!running)
            return true;
        running = false;
        return pthread_join(thread_id, NULL) == 0;
    }

    bool kill() {
        if (!running)
            return true;
        pthread_exit(NULL);
        return false;
    }

    bool is_running() {
        return running;
    }

protected:
    virtual void run() = 0;
    static void* run_handler(void* p) {
        BaseThread* callback_function = (BaseThread*) p;
        callback_function->run();
        return 0;
    }

    bool running;
    pthread_t thread_id;
};

struct SendTask{
    uint8_t channel_id;
    uint64_t length;
    char* data;
};

// Sends a message on a particular channel
class SendThread: public BaseThread {
public:
    SendThread(emp::NetIO* io) {
        this->io = io;
    }

    ~SendThread() {
        signal_end();
        this->wait();
    }

    void add_task(uint8_t channel_id, uint64_t length, const char* data) {
        SendTask task;
        task.channel_id = channel_id;
        task.length = length;
        task.data = (char*) malloc(task.length);
        memcpy(task.data, data, task.length);

        tasks.push(task);
    }

    void signal_end() {
        char dummy_val;
        uint8_t channel_id = ADMIN_CHANNEL;
        add_task(channel_id, 0, &dummy_val);
    }

    uint64_t get_send_count() {
        return io->send_counter;
    }

    void run() {
        uint8_t channel_id;
        bool run = true;
        while(run) {
            SendTask task = tasks.pop();

            channel_id = task.channel_id;
            io->send_data(&channel_id, sizeof(uint8_t), false);
            io->send_data(&task.length, sizeof(uint64_t), false);
            if(task.length > 0) {
                io->send_data(task.data, task.length, false);
            }

            free(task.data);

            if(channel_id == ADMIN_CHANNEL) {
                run = false;
            }
        }
    }

private:
    emp::NetIO* io;
    ConcurrentQueue<SendTask> tasks;
};

struct RecvTask {
    char *data;
    uint64_t length;
};

// Listens for messages on num_threads many channels, and stores them in the
// corresponding concurrent queues, which can later be retrieved using channel_id
class RecvThread: public BaseThread {
public:
    RecvThread(emp::NetIO* io, uint8_t num_threads) {
        this->io = io;
        this->num_channels = num_threads;
        this->listeners.resize(num_channels);
        for(uint8_t i = 0; i < num_channels; i++) {
            listeners[i] = new ConcurrentQueue<RecvTask>();
        }
    }

    ~RecvThread() {
        this->wait();
        for(uint8_t i = 0; i < num_channels; i++) {
            listeners[i]->flush();
            delete listeners[i];
        }
        listeners.clear();
    }

    RecvTask get_task(uint8_t channel_id){
        RecvTask task = listeners[channel_id]->pop();
        return task;
    }

    uint64_t get_recv_count() {
        return io->recv_counter;
    }

    void run() {
        uint8_t channel_id;
        uint64_t length;
        uint64_t recv_len;
        while(true) {
            recv_len = 0;
            recv_len += io->recv_data(&channel_id, sizeof(uint8_t), false);
            recv_len += io->recv_data(&length, sizeof(uint64_t), false);

            if(recv_len > 0) {
                if(channel_id == ADMIN_CHANNEL) {
                    char* data = (char*) malloc(length);
                    io->recv_data(data, length, false);
                    free(data);
                    return;
                }
                else {
                    RecvTask task;
                    task.data = (char*) malloc(length);
                    task.length = length;

                    io->recv_data(task.data, length, false);
                    listeners[channel_id]->push(task);
                }
            } else {
                // We received 0 bytes, probably due to some major error. Just return.
                return;
            }
        }
    }

private:
    emp::NetIO* io;
    uint8_t num_channels;
    std::vector<ConcurrentQueue<RecvTask>*> listeners;
};
#endif //PQ_OT_IO_THREAD_H__
