#ifndef RSJFW_COMMON_H
#define RSJFW_COMMON_H
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace rsjfw
{
    using ProgressCallback = std::function<void(float progress, const std::string& message)>;
    static ProgressCallback makeSubProgress(float progStart, float progEnd, const std::string& title, ProgressCallback cb)
    {
        if (!cb) return nullptr;
        return [=](float p, const std::string& text)
        {
            p = std::clamp(p, 0.0f, 1.0f);
            float percent = progStart + p * (progEnd - progStart);
            cb(percent, title + ": " + text);
        };
    }

    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static std::string str_vec_to_str_space(std::vector<std::string> list)
    {
        std::string final{};
        for (auto e : list) final += e + " ";
        return final;
    }

    static std::string str_vec_to_str_list(std::vector<std::string> list)
    {
        std::string final{};
        for (auto e : list) final += e + ", ";
        return final;
    }
}

#endif
