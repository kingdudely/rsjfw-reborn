#ifndef STREAMBUF_H
#define STREAMBUF_H

#include <functional>
#include <mutex>
#include <streambuf>
#include <vector>

typedef class stream_buffer {
public:
  using callback = std::function<void(std::string_view)>;
  void append(std::string_view chunk);
  std::string view() const;
  void connect(callback listener);

  stream_buffer() : mutex_(), buffer_(), listeners_() {};
  ~stream_buffer() = default;

private:
  mutable std::mutex mutex_;
  std::string buffer_;
  std::vector<callback> listeners_;
} stream_buffer_t;

#endif