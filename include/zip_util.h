#ifndef ZIP_UTIL_H
#define ZIP_UTIL_H

#include <string>

#include "common.h"

namespace rsjfw {
    class ZipUtil {
    public:
        static bool extract(const std::string& archivePath, const std::string& destPath, ProgressCallback cb = nullptr);
    };
}
#endif