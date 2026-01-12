#ifndef DIAGNOSTICS_VIEW_H
#define DIAGNOSTICS_VIEW_H

#include "gui/view.h"

namespace rsjfw {

class DiagnosticsView : public View {
public:
    void render() override;
    const char* getName() const override { return "Diagnostics"; }
};

}

#endif
