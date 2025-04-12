#include "buffer.h"
using std::string;

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 使用data()函数获取缓冲区的起始地址
char* Buffer::BeginPtr_() {
    return buffer_.data();
}

const char* Buffer::BeginPtr_() const {
    return buffer_.data();
}

size_t Buffer::ReadableBytes() const {
    if (writePos_ >= readPos_) {
        return writePos_ - readPos_;
    }
    return buffer_.size() - readPos_ + writePos_;
}

size_t Buffer::WritableBytes() const {
    // 保留一个字节来区分空和满的情况
    if (writePos_ >= readPos_) {
        return buffer_.size() - writePos_ + readPos_ - 1;
    }
    // 写位置在读位置前面，直接计算差值减一
    return readPos_ - writePos_ - 1;
}

size_t Buffer::PrependableBytes() const {
    return readPos_;
}


const char* Buffer::Peek() const {// 返回可读数据的起始地址
    return BeginPtr_() + readPos_;
}

// retrieve函数用于从缓冲区中取出数据
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ = (readPos_ + len) % buffer_.size();

    // 如果缓冲区变空，重置读写位置以最大化连续可写空间
    if (readPos_ == writePos_) {
        readPos_ = writePos_ = 0;
    }
}

void Buffer::RetrieveUntil(const char* end) {
    if (end >= Peek()) {// 连续空间
        Retrieve(end - Peek());
    } else {// 处理环绕的情况
        size_t endOffset = end - BeginPtr_();
        size_t readOffset = readPos_;
        size_t len = buffer_.size() - readOffset + endOffset;
        Retrieve(len);
    }
}

void Buffer::RetrieveAll() {
    readPos_ = writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string result;
    size_t readable = ReadableBytes();
    result.reserve(readable);// res

    if (writePos_ >= readPos_) {
        result.append(BeginPtr_() + readPos_, readable);
    } else {
        size_t firstPart = buffer_.size() - readPos_;
        result.append(BeginPtr_() + readPos_, firstPart);
        result.append(BeginPtr_(), writePos_);
    }

    RetrieveAll();
    return result;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    assert(len <= WritableBytes());
    writePos_ = (writePos_ + len) % buffer_.size();
}

void Buffer::EnsureWriteable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

// Append函数用于向缓冲区中添加数据
void Buffer::Append(const string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);


    size_t remainingSpace = buffer_.size() - writePos_;
    size_t firstPartLen = std::min(len, remainingSpace);
    std::copy(str, str + firstPartLen, BeginPtr_() + writePos_);

    // 处理可能的环绕
    if (firstPartLen < len) {
        std::copy(str + firstPartLen, str + len, BeginPtr_());
    }

    HasWritten(len);
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char extrabuff[65535];
    iovec iov[2];

    // 确定连续可写区域大小
    size_t writable = WritableBytes();
    size_t contiguous_writable;

    if (writePos_ + writable > buffer_.size()) {
        contiguous_writable = buffer_.size() - writePos_;
    } else {
        contiguous_writable = writable;
    }

    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = contiguous_writable;
    iov[1].iov_base = extrabuff;
    iov[1].iov_len = sizeof(extrabuff);

    const ssize_t len = readv(fd, iov, 2);

    if (len < 0) {
        *saveErrno = errno;
    } else if (static_cast<size_t>(len) <= contiguous_writable) {
        HasWritten(len);
    } else {
        // 缓冲区已满，部分数据在extrabuff中
        size_t buffer_filled = contiguous_writable;
        size_t extra_data = len - buffer_filled;
        HasWritten(buffer_filled);

        // 确保有足够空间来附加额外数据
        Append(extrabuff, extra_data);
    }

    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readable = ReadableBytes();
    if (readable == 0) return 0;

    ssize_t total_written = 0;

    if (writePos_ > readPos_) {
        // 数据是连续的
        ssize_t len = write(fd, Peek(), readable);
        if (len < 0) {
            *saveErrno = errno;
            return len;
        }

        Retrieve(len);
        total_written = len;
    } else {
        // 数据不连续，分两次写
        size_t first_part = buffer_.size() - readPos_;
        ssize_t len1 = write(fd, Peek(), first_part);

        if (len1 < 0) {
            *saveErrno = errno;
            return len1;
        }

        Retrieve(len1);
        total_written = len1;

        if (static_cast<size_t>(len1) == first_part && writePos_ > 0) {
            // 写入第二部分
            ssize_t len2 = write(fd, Peek(), writePos_);
            if (len2 < 0) {
                *saveErrno = errno;
                // 已经写了一部分，返回已写数量
                return total_written;
            }

            Retrieve(len2);
            total_written += len2;
        }
    }

    return total_written;
}



void Buffer::MakeSpace_(size_t len) {
    // 检查现有空间是否足够
    if (WritableBytes() + PrependableBytes() < len + 1) {
        // 空间不足，需要扩容
        size_t readable = ReadableBytes();
        size_t newSize = (buffer_.size() + len) * 2;  // 扩容至少两倍
        std::vector<char> newBuffer(newSize);

        // 复制数据到新缓冲区，保持线性布局
        if (readPos_ <= writePos_) {
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, newBuffer.data());
        } else {
            size_t first_part = buffer_.size() - readPos_;
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + buffer_.size(), newBuffer.data());
            std::copy(BeginPtr_(), BeginPtr_() + writePos_, newBuffer.data() + first_part);
        }

        buffer_ = std::move(newBuffer);
        readPos_ = 0;
        writePos_ = readable;
    } else {
        // 可以通过移动数据来获取足够空间
        size_t readable = ReadableBytes();

        // 创建临时缓冲区存放所有可读数据
        std::vector<char> temp(readable);

        if (readPos_ <= writePos_) {
            // 数据连续
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, temp.data());
        } else {
            // 数据不连续
            size_t first_part = buffer_.size() - readPos_;
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + buffer_.size(), temp.data());
            std::copy(BeginPtr_(), BeginPtr_() + writePos_, temp.data() + first_part);
        }

        // 复制回缓冲区头部
        std::copy(temp.begin(), temp.end(), BeginPtr_());
        readPos_ = 0;
        writePos_ = readable;
    }
}

size_t Buffer::NextPos(size_t pos, size_t len) const {
    return (pos + len) % buffer_.size();
}