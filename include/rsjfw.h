#ifndef RSJFW_H
#define RSJFW_H

#include <filesystem>
#include <string>

#define RSJFW_VERSION 11.37511

namespace rsjfw {
enum class pkg_type { appimage, flatpak, bin };
static pkg_type type;
static std::filesystem::path root;
static std::filesystem::path bin;
static std::filesystem::path lib;

}

#endif