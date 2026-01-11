#include "streambuf.h"

void stream_buffer::append(std::string_view chunk) {
    std::lock_guard lock(mutex_);
    buffer_ += chunk;
    for (auto &cb : listeners_) cb(chunk);
}

std::string stream_buffer::view() const {
    std::lock_guard lock(mutex_);
    return buffer_;
}

void stream_buffer::connect(callback listener) {
    std::lock_guard lock(mutex_);
    listeners_.push_back(listener);
}
