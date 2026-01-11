#include "logger.h"

#include <iostream>
#include <cstdarg>
#include <algorithm>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctime>
#include <vector>
#include <cmath>
#include <sstream>

namespace rsjfw {

Logger& Logger::instance() {
    static Logger l;
    return l;
}

Logger::Logger() {
    file_.open("app.log", std::ios::out | std::ios::app);
}

Logger::~Logger() {
    if (activeLines_ > 0) {
        clearProgressLines();
    }
    if (file_.is_open()) file_.close();
}

int Logger::termWidth() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
        return 80;
    }
    return w.ws_col;
}

std::string Logger::getTimestamp() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);
    return std::string(buf);
}

std::string Logger::getLevelString(Level lvl) {
    switch(lvl) {
        case ERR:   return "[ERROR]";
        case WARN:  return "[WARN ]";
        case INFO:  return "[INFO ]";
        case DEBUG: return "[DEBUG]";
        default:    return "[UNKNOWN]";
    }
}

std::string Logger::getColor(Level lvl) {
    switch(lvl) {
        case ERR:   return "\033[31m";
        case WARN:  return "\033[33m";
        case INFO:  return "\033[36m";
        case DEBUG: return "\033[90m";
        default:    return "\033[0m";
    }
}

void Logger::clearProgressLines() {
    if (activeLines_ <= 0) return;
    std::cout << "\r";
    std::cout << "\033[" << activeLines_ << "A";
    std::cout << "\033[J";
    std::cout << std::flush;

    activeLines_ = 0;
}

void Logger::drawBars() {
    if (bars_.empty()) {
        activeLines_ = 0;
        return;
    }

    int w = termWidth();
    int safeWidth = w - 5;
    if (safeWidth < 20) safeWidth = 20;

    std::stringstream ss;
    int count = 0;

    for (const auto& b : bars_) {
        int percentVal = static_cast<int>(b.percent * 100);
        std::string percentStr = " " + std::to_string(percentVal) + "%";

        int reserved = 4 + (int)percentStr.length();
        int maxTitleLen = safeWidth / 3;

        std::string displayTitle = b.title;
        if ((int)displayTitle.length() > maxTitleLen) {
            displayTitle = displayTitle.substr(0, maxTitleLen - 3) + "...";
        }

        int barSpace = safeWidth - (int)displayTitle.length() - reserved;
        if (barSpace < 5) barSpace = 5;

        int fill = static_cast<int>(b.percent * barSpace);
        fill = std::clamp(fill, 0, barSpace);

        ss << "\033[1m" << displayTitle << "\033[0m " << "[";

        if (b.percent < 0.3f) ss << "\033[31m";
        else if (b.percent < 0.7f) ss << "\033[33m";
        else ss << "\033[32m";

        if (fill > 0) ss << std::string(fill, '=');
        if (fill < barSpace) ss << ">";
        if (barSpace > fill + 1) ss << std::string(barSpace - fill - 1, ' ');

        ss << RESET << "]" << percentStr << "\n";
        count++;
    }

    std::cout << ss.str() << std::flush;
    activeLines_ = count;
}

void Logger::log(Level lvl, const char* file, const char* func, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(mtx_);

    clearProgressLines();

    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    std::string timestamp = getTimestamp();
    std::string levelStr = getLevelString(lvl);
    std::string colorCode = getColor(lvl);


    std::string filename = file;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1);
    }

    std::cout << "\033[90m" << timestamp << RESET << " "
              << colorCode << levelStr << RESET << " "
              << "\033[90m[" << filename << ":" << func << "]\033[0m "
              << buf << "\n";

    if (file_.is_open()) {
        file_ << timestamp << " " << levelStr << " [" << filename << ":" << func << "] " << buf << "\n";
    }

    drawBars();
}

int Logger::createProgressBar(const std::string& title) {
    std::lock_guard<std::mutex> lock(mtx_);
    clearProgressLines();

    ProgressBar b;
    b.id = bars_.empty() ? 0 : bars_.back().id + 1;
    b.percent = 0.f;
    b.title = title;
    b.done = false;
    bars_.push_back(b);

    drawBars();
    return b.id;
}

void Logger::updateProgress(int id, float percent) {
    std::lock_guard<std::mutex> lock(mtx_);

    bool found = false;
    for (auto& b : bars_) {
        if (b.id == id) {
            float newP = std::clamp(percent, 0.f, 1.f);
            if (std::abs(b.percent - newP) < 0.001f) return;

            b.percent = newP;
            found = true;
            break;
        }
    }

    if (found) {
        clearProgressLines();
        drawBars();

        for (auto it = bars_.begin(); it != bars_.end(); ) {
             if (it->id == id && it->percent >= 1.0f) {
                 std::string title = it->title;
                 std::string timestamp = getTimestamp();

                 bars_.erase(it);
                 clearProgressLines();
                 std::cout << "\033[90m" << timestamp << RESET << " "
                           << "\033[32m[DONE ]\033[0m "
                           << title << " - 100%\n";

                 if (file_.is_open()) {
                     file_ << timestamp << " [DONE ] " << title << " - 100%\n";
                 }

                 drawBars();
                 return;
             } else {
                 ++it;
             }
        }
    }
}

void Logger::updateProgressTitle(int id, const std::string& title) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& b : bars_) {
        if (b.id == id) {
            b.title = title;
            break;
        }
    }
    clearProgressLines();
    drawBars();
}

void Logger::endProgress(int id) {
    std::lock_guard<std::mutex> lock(mtx_);

    clearProgressLines();

    std::string doneTitle = "Unknown";
    auto it = bars_.begin();
    while (it != bars_.end()) {
        if (it->id == id) {
            doneTitle = it->title;
            it = bars_.erase(it);
        } else {
            ++it;
        }
    }

    std::string timestamp = getTimestamp();
    std::cout << "\033[90m" << timestamp << RESET << " "
              << "\033[32m[DONE ]\033[0m "
              << doneTitle << " - 100%\n";

    if (file_.is_open()) {
        file_ << timestamp << " [DONE ] " << doneTitle << " - 100%\n";
    }

    drawBars();
}

}