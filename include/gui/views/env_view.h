#ifndef ENV_VIEW_H
#define ENV_VIEW_H

#include "gui/view.h"
#include <map>
#include <string>

namespace rsjfw {

class EnvView : public View {
public:
    void render() override;
    const char* getName() const override { return "Environment"; }

private:
    std::map<std::string, std::string> cachedBaseEnv_;
    bool envLoaded_ = false;
    void refreshEnv();
};

}

#endif
