#define STB_IMAGE_IMPLEMENTATION
#include "async_image_loader.h"
#include "http.h"
#include "logger.h"
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <thread>

namespace rsjfw {

AsyncImageLoader& AsyncImageLoader::instance() {
    static AsyncImageLoader inst;
    return inst;
}

unsigned int AsyncImageLoader::getTexture(const std::string& url) {
    if (url.empty()) return 0;
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(url);
    if (it != cache_.end()) {
        auto& state = it->second;
        if (state.pendingUpload) {
            state.textureId = uploadTexture(state.pixels, state.width, state.height);
            state.pixels.clear(); 
            state.pendingUpload = false;
        }
        return state.textureId;
    }

    auto& state = cache_[url];
    if (!state.loading) {
        state.loading = true;
        std::thread(&AsyncImageLoader::loadImage, this, url).detach();
    }
    
    return 0;
}

void AsyncImageLoader::loadImage(const std::string& url) {
    try {
        std::string rawData = HTTP::get(url);
        if (rawData.empty()) return;

        int width, height, channels;
        unsigned char* img = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(rawData.data()), 
            rawData.size(), &width, &height, &channels, 4);
        
        if (img) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& state = cache_[url];
            state.width = width;
            state.height = height;
            state.pixels.assign(img, img + (width * height * 4));
            state.pendingUpload = true;
            stbi_image_free(img);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load image from %s: %s", url.c_str(), e.what());
    }
}

unsigned int AsyncImageLoader::uploadTexture(const std::vector<unsigned char>& pixels, int width, int height) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return id;
}

}
