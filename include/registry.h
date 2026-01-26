#ifndef RSJFW_REGISTRY_H
#define RSJFW_REGISTRY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <shared_mutex>
#include <filesystem>

namespace rsjfw {

struct CaseInsensitiveLess {
    bool operator()(const std::string& s1, const std::string& s2) const {
        return std::lexicographical_compare(
            s1.begin(), s1.end(),
            s2.begin(), s2.end(),
            [](unsigned char c1, unsigned char c2) {
                return std::tolower(c1) < std::tolower(c2);
            }
        );
    }
};

enum class RegistryType {
    None = 0,
    String = 1,
    ExpandString = 2,
    Binary = 3,
    Dword = 4,
    DwordBE = 5,
    Link = 6,
    MultiString = 7,
    Qword = 11,
    Custom = 100,
    Unknown = 99
};

struct RegistryValue {
    std::string name;
    RegistryType type;
    std::vector<uint8_t> data;
    uint32_t customType = 0;

    std::string asString() const;
    uint32_t asDword() const;
    std::vector<std::string> asMultiString() const;
};

class RegistryKey {
public:
    std::string name;
    std::vector<RegistryValue> values;
    std::vector<std::shared_ptr<RegistryKey>> subkeys;
    
    RegistryKey* parent = nullptr;
    uint64_t modified = 0;
    bool isLink = false;
    uint32_t unixTime = 0;
    bool wasInFile = false;

    RegistryKey() : wasInFile(false) {}
    RegistryKey(const std::string& n, RegistryKey* p = nullptr) : name(n), parent(p), wasInFile(false) {}

    RegistryValue* getValue(const std::string& name);
    void setValue(const std::string& name, RegistryType type, const std::vector<uint8_t>& data, uint32_t customType = 0);
    bool deleteValue(const std::string& name);

    RegistryKey* add(const std::string& path);
    RegistryKey* query(const std::string& path);
    bool deleteKey(const std::string& path);

    RegistryKey* queryPath(const std::string& path, bool create);
    RegistryKey* root();
    void copyFrom(const RegistryKey& other);

    bool load(std::istream& is);
    bool save(std::ostream& os, const std::string& rootPath);

private:
    std::string unescape(const std::string& s);
    std::string escape(const std::string& s);
    std::string unquote(const std::string& s);
    std::optional<RegistryValue> parseData(const std::string& value);
};

class Registry {
public:
    Registry(const std::string& prefixDir);
    ~Registry();

    std::optional<std::string> query(const std::string& path, const std::string& valueName);
    void add(const std::string& path, const std::string& name, const std::string& val, const std::string& type = "REG_SZ");
    void transplant(const std::string& path, Registry& source);
    bool commit();
    
    std::shared_ptr<RegistryKey> getCurrentUser() { return currentUser_; }

private:
    std::string prefixDir_;
    std::shared_ptr<RegistryKey> machine_;
    std::shared_ptr<RegistryKey> currentUser_;
    mutable std::shared_mutex mutex_;
    
    std::filesystem::file_time_type lastSystem_;
    std::filesystem::file_time_type lastUser_;

    std::string systemRelativePath_ = "REGISTRY\\Machine";
    std::string userRelativePath_ = "REGISTRY\\User\\S-1-5-21-0-0-0-1000";

    void checkAndReload();
    bool loadHive(const std::string& filename, std::shared_ptr<RegistryKey>& key);
    bool saveHive(const std::string& filename, std::shared_ptr<RegistryKey>& key, const std::string& rootPath);
    RegistryKey* getRoot(const std::string& path, std::string& subPath);
};

} // namespace rsjfw

#endif // RSJFW_REGISTRY_H
