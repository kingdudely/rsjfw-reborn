#ifndef HTTP_H
#define HTTP_H

#include <string>
#include "common.h"

#include <curl/curl.h>

namespace rsjfw {

    class HTTP {
    public:
        static std::string get(const std::string& url);
        static bool download(const std::string& url, const std::string& destPath, ProgressCallback cb = nullptr);

    private:
        static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
        static size_t fileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
        static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    };

}
#endif