#ifndef TROUBLESHOOT_VIEW_H
#define TROUBLESHOOT_VIEW_H

#include "gui/view.h"

namespace rsjfw {

class TroubleshootView : public View {
public:
    void render() override;
    const char* getName() const override { return "Troubleshooting"; }
};

}

#endif
