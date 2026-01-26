#ifndef ROBLOX_API_H
#define ROBLOX_API_H

#include <string>
#include <vector>

namespace rsjfw {

    struct RobloxPackage {
        std::string name;
        std::string checksum;
        size_t size;
        size_t packedSize;
    };

    struct RobloxUserInfo {
        std::string userId;
        std::string username;
        std::string displayName;
        std::string profilePicUrl;
    };

    class RobloxAPI {
    public:
        static std::string getLatestVersionGUID(const std::string& channel = "LIVE");
        static std::vector<RobloxPackage> getPackageManifest(const std::string& versionGUID);
        static RobloxUserInfo getUserInfo(const std::string& userId);

        static const std::string BASE_URL;
    };

}
#endif
