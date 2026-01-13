#include "http.h"
#include "logger.h"
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

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
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastTime;
        curl_off_t lastDlTotal = 0;
        double stalledTime = 0;
        bool aborted = false;
    };

    int HTTP::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
    {
        auto* data = static_cast<ProgData*>(clientp);
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - data->lastTime).count();

        if (elapsedMs >= 500)
        {
            double delta = std::max(0.0, (double)dlnow - (double)data->lastDlTotal);
            double speed = (delta / (double)elapsedMs) * 1000.0;
            
            data->lastDlTotal = dlnow;
            data->lastTime = now;

            if (delta <= 0 && dlnow > 0 && dlnow < dltotal) {
                data->stalledTime += (double)elapsedMs;
            } else {
                data->stalledTime = 0;
            }

            if (data->stalledTime >= 30000.0) {
                data->aborted = true;
                return 1;
            }

            if (data->cb && dltotal > 0)
            {
                float prog = static_cast<float>((double)dlnow / (double)dltotal);
                prog = std::clamp(prog, 0.0f, 1.0f);
                
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1);
                if (speed < 1024) ss << (int)speed << " B/s";
                else if (speed < 1024 * 1024) ss << (speed / 1024.0) << " KB/s";
                else ss << (speed / (1024.0 * 1024.0)) << " MB/s";

                data->cb(prog, ss.str());
            }
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

        int retries = 3;
        while (retries--) {
            std::ofstream ofs(partPath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) {
                LOG_ERROR("Failed to open file for writing: %s", partPath.c_str());
                return false;
            }

            CURL* curl = curl_easy_init();
            if (!curl) return false;

            ProgData pd{cb, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()};

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "RSJFW/2.0");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

            if (cb) {
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pd);
            }

            CURLcode res = curl_easy_perform(curl);
            ofs.flush();
            ofs.close();
            curl_easy_cleanup(curl);

            if (res == CURLE_OK) {
                std::error_code ec;
                fs::rename(partPath, finalPath, ec);
                if (ec) {
                    LOG_ERROR("Rename failed: %s", ec.message().c_str());
                    fs::remove(partPath);
                    return false;
                }
                return true;
            } else {
                fs::remove(partPath);
                if (retries > 0) {
                    LOG_WARN("Download failed (%s), retrying...", curl_easy_strerror(res));
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
                LOG_ERROR("Download failed after retries: %s", url.c_str());
                return false;
            }
        }
        return false;
    }
}
