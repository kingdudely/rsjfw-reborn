#ifndef VIEW_H
#define VIEW_H

#include <string>

namespace rsjfw {

class View {
public:
    virtual ~View() = default;
    virtual void render() = 0;
    virtual const char* getName() const = 0;
};

}

#endif
