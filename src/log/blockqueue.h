#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <cassert>

template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);
    ~BlockDeque();

    void clear();
    bool empty() const;
    bool full() const;
    void Close();

    size_t size() const;
    size_t capacity() const;
    T front() const;
    T back() const;

    void push_back(const T &item);
    void push_back(T &&item);
    void push_front(const T &item);
    void push_front(T &&item);
    bool pop(T &item);
    bool pop(T &item, int timeout);
    void flush();
private:
    std::deque<T> deq_;

    size_t capacity_;
    mutable std::mutex mtx_;
    std::atomic<bool> isClose_;

    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) : capacity_(MaxCapacity), isClose_(false) {
    assert(MaxCapacity > 0);
}
template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}
template<class T>
T BlockDeque<T>::front() const {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}
template<class T>
T BlockDeque<T>::back() const {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}
template<class T>
size_t BlockDeque<T>::size() const {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}
template<class T>
size_t BlockDeque<T>::capacity() const {
    return capacity_;
}
template<class T>
bool BlockDeque<T>::empty() const {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}
template<class T>
bool BlockDeque<T>::full() const {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {//队尾插入
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_ &&!isClose_) {
        condProducer_.wait(locker);
    }
    if (isClose_) {
        return;
    }
    try {
        deq_.push_back(item);
    } catch (...) {
        condProducer_.notify_one();
        throw;
    }
    condConsumer_.notify_one();
}
template<class T>
void BlockDeque<T>::push_back(T &&item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_ &&!isClose_) {
        condProducer_.wait(locker);
    }
    if (isClose_) {
        return;
    }
    try {
        deq_.push_back(std::move(item));
    } catch (...) {
        condProducer_.notify_one();
        throw;
    }
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {//队首插入
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_ &&!isClose_) {
        condProducer_.wait(locker);
    }
    if (isClose_) {
        return;
    }
    try {
        deq_.push_front(item);
    } catch (...) {
        condProducer_.notify_one();
        throw;
    }
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(T &&item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_ &&!isClose_) {
        condProducer_.wait(locker);
    }
    if (isClose_) {
        return;
    }
    try {
        deq_.push_front(std::move(item));
    } catch (...) {
        condProducer_.notify_one();
        throw;
    }
    condConsumer_.notify_one();
}
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 修改循环条件，添加 isClose_ 判断
    while (deq_.empty() || isClose_) {
        if (isClose_ && deq_.empty()) {
            return false;
        }
        condConsumer_.wait(locker);
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 修改循环条件，添加 isClose_ 判断
    while (deq_.empty() || isClose_) {
        if (isClose_ && deq_.empty()) {
            return false;
        }
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H