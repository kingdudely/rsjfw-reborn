#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>

namespace rsjfw {

    class Logger {
    public:
        enum Level { ERR, WARN, INFO, DEBUG };

        static Logger& instance();


        void log(Level lvl, const char* file, const char* func, const char* fmt, ...);

        struct ProgressBar {
            int id;
            float percent;
            std::string title;
            bool done;
        };

        int createProgressBar(const std::string& title);
        void updateProgress(int id, float percent);
        void updateProgressTitle(int id, const std::string& title);
        void endProgress(int id);

    private:
        Logger();
        ~Logger();


        void clearProgressLines();
        void drawBars();
        int termWidth();
        std::string getTimestamp();
        std::string getLevelString(Level lvl);
        std::string getColor(Level lvl);

        std::mutex mtx_;
        std::ofstream file_;
        std::vector<ProgressBar> bars_;


        int activeLines_ = 0;

        const std::string RESET = "\033[0m";
    };


#define LOG_ERROR(fmt, ...) rsjfw::Logger::instance().log(rsjfw::Logger::ERR, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  rsjfw::Logger::instance().log(rsjfw::Logger::WARN,  __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  rsjfw::Logger::instance().log(rsjfw::Logger::INFO,  __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) rsjfw::Logger::instance().log(rsjfw::Logger::DEBUG, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define PROG_CREATE(title) rsjfw::Logger::instance().createProgressBar(title)
#define PROG_UPDATE(id, percent) rsjfw::Logger::instance().updateProgress(id, percent)
#define PROG_UPDATE_TITLE(id, title) rsjfw::Logger::instance().updateProgressTitle(id, title)
#define PROG_END(id) rsjfw::Logger::instance().endProgress(id)

}

#endif