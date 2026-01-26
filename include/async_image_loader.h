#ifndef ASYNC_IMAGE_LOADER_H
#define ASYNC_IMAGE_LOADER_H

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>

namespace rsjfw {

class AsyncImageLoader {
public:
    static AsyncImageLoader& instance();

    // Returns texture ID if loaded, 0 otherwise
    // Must be called from the main thread (GL thread)
    unsigned int getTexture(const std::string& url);

private:
    AsyncImageLoader() = default;
    
    struct ImageState {
        unsigned int textureId = 0;
        std::atomic<bool> loading{false};
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
        bool pendingUpload = false;
    };

    std::map<std::string, ImageState> cache_;
    std::mutex mutex_;

    void loadImage(const std::string& url);
    unsigned int uploadTexture(const std::vector<unsigned char>& pixels, int width, int height);
};

}

#endif
