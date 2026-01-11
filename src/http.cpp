#include "http.h"
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace rsjfw
{
    namespace fs = std::filesystem;

    size_t HTTP::fileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
    {
        auto* ofs = static_cast<std::ofstream*>(userp);
        size_t total = size * nmemb;
        ofs->write(static_cast<char*>(contents), total);
        return total;
    }

    size_t HTTP::writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
    {
        size_t total = size * nmemb;
        userp->append((char*)contents, total);
        return total;
    }

    struct ProgData
    {
        ProgressCallback cb;
    };

    int HTTP::progressCallback(void* clientp, double dltotal, double dlnow, double, double)
    {
        auto* data = static_cast<ProgData*>(clientp);
        if (data->cb && dltotal > 0.0)
        {
            float prog = static_cast<float>(dlnow / dltotal);
            prog = std::clamp(prog, 0.0f, 1.0f);
            data->cb(prog, "Downloading " + std::to_string(int(prog * 100)) + "%");
        }
        return 0;
    }

    std::string HTTP::get(const std::string& url)
    {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("CURL init failed");
        std::string resp;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RSJFW/2.0");
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) throw std::runtime_error("CURL GET failed: " + url);
        return resp;
    }

    bool HTTP::download(const std::string& url,
                    const std::string& dest,
                    ProgressCallback cb)
    {
        fs::path finalPath = dest;
        fs::path partPath = dest + ".part";


        fs::create_directories(finalPath.parent_path());

        std::ofstream ofs(partPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            std::cerr << "[HTTP] Failed to open file: " << partPath << "\n";
            return false;
        }

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        ProgData pd{cb};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RSJFW/2.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

        if (cb) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progressCallback);
            curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pd);
        }

        CURLcode res = curl_easy_perform(curl);
        ofs.flush();
        ofs.close();
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "[HTTP] CURL error: " << curl_easy_strerror(res) << "\n";
            fs::remove(partPath);
            return false;
        }


        std::error_code ec;
        fs::rename(partPath, finalPath, ec);
        if (ec) {
            std::cerr << "[HTTP] Rename failed: " << ec.message() << "\n";
            fs::remove(partPath);
            return false;
        }

        return true;
    }
}
