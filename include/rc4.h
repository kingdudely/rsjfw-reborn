#ifndef RC4_H
#define RC4_H

#include <vector>
#include <cstdint>
#include <algorithm>

namespace rsjfw {

class RC4 {
public:
    RC4(const std::vector<uint8_t>& key) {
        for (int i = 0; i < 256; i++) s[i] = i;
        int j = 0;
        for (int i = 0; i < 256; i++) {
            j = (j + s[i] + key[i % key.size()]) % 256;
            std::swap(s[i], s[j]);
        }
        i_ = j_ = 0;
    }

    void xorStream(std::vector<uint8_t>& data) {
        for (size_t k = 0; k < data.size(); k++) {
            i_ = (i_ + 1) % 256;
            j_ = (j_ + s[i_]) % 256;
            std::swap(s[i_], s[j_]);
            data[k] ^= s[(s[i_] + s[j_]) % 256];
        }
    }

private:
    uint8_t s[256];
    int i_, j_;
};

}

#endif
