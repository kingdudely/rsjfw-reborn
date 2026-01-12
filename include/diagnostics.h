#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace rsjfw {

struct HealthStatus {
    bool ok;
    std::string message;
    std::string detail;
    bool fixable = false;
    bool ignored = false;
    std::function<void(std::function<void(float, std::string)>)> fixAction;
    std::string category = "General";
};

class Diagnostics {
public:
    static Diagnostics& instance();

    void runChecks();
    const std::vector<std::pair<std::string, HealthStatus>>& getResults() const { return results_; }
    int failureCount() const;
    void fixIssue(const std::string& name, std::function<void(float, std::string)> progressCb);

private:
    Diagnostics() = default;
    
    void checkDependencies();
    void checkDesktopFile();
    void checkProtocolHandler();


    std::vector<std::pair<std::string, HealthStatus>> results_;
    std::mutex mtx_;
};

}

#endif
