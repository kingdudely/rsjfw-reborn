#ifndef FFLAGS_VIEW_H
#define FFLAGS_VIEW_H

#include "gui/view.h"

namespace rsjfw {

class FFlagsView : public View {
public:
    void render() override;
    const char* getName() const override { return "FFlags"; }
};

}

#endif
