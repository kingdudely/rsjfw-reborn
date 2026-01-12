#ifndef GENERAL_VIEW_H
#define GENERAL_VIEW_H

#include "gui/view.h"

namespace rsjfw {

class GeneralView : public View {
public:
    void render() override;
    const char* getName() const override { return "General"; }
};

}

#endif
