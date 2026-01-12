#ifndef ENV_VIEW_H
#define ENV_VIEW_H

#include "gui/view.h"

namespace rsjfw {

class EnvView : public View {
public:
    void render() override;
    const char* getName() const override { return "Env Vars"; }
};

}

#endif
