#include "prefix.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string_view>

namespace rsjfw {

namespace fs = std::filesystem;

Prefix::Prefix(const std::string& root, const std::string& install)
    : rootDir_(root), installDir_(install) {
    fs::create_directories(rootDir_);
}

void Prefix::setExecutor(const std::string& binary, const std::vector<std::string>& preArgs) {
    executorBinary_ = binary;
    executorPreArgs_ = preArgs;
}

void Prefix::setWrapper(const std::vector<std::string>& wrapper) {
    wrapperArgs_ = wrapper;
}

void Prefix::setEnvironment(const std::map<std::string, std::string>& env) {
    baseEnv_ = env;
}

std::string Prefix::resolveBinary(const std::string& binary) const {
    if (!executorBinary_.empty()) return executorBinary_;

    fs::path root(installDir_);
    std::vector<fs::path> searchPaths = {
        root / binary,
        root / "bin" / binary,
        root / "files" / "bin" / binary
    };
    
    for (const auto& p : searchPaths) {
        if (fs::exists(p)) return p.string();
    }
    return binary;
}

void Prefix::addLibPaths(cmd::Options& opts) const {
    if (!executorBinary_.empty()) return;

    std::string libPath = "";
    if (char* env_p = std::getenv("LD_LIBRARY_PATH")) libPath = env_p;
    
    fs::path r(installDir_);
    std::vector<std::string> libDirs = {
        "lib", "lib64", 
        "lib/wine", "lib64/wine",
        "lib/wine/x86_64-unix", "lib/wine/i386-unix",
        "files/lib", "files/lib64",
        "files/lib/x86_64-linux-gnu", "files/lib/i386-linux-gnu",
        "files/lib/wine/x86_64-unix", "files/lib/wine/i386-unix"
    };
    for (const auto& d : libDirs) {
        auto p = r / d;
        if (fs::exists(p)) {
            if(!libPath.empty()) libPath = ":" + libPath;
            libPath = p.string() + libPath;
        }
    }
    if (!libPath.empty()) opts.env["LD_LIBRARY_PATH"] = libPath;
}

bool Prefix::init(ProgressCb cb) {
    if (cb) cb(0.1f, "Checking prefix...");

    if (!fs::exists(fs::path(rootDir_) / "system.reg")) {
        if (cb) cb(0.3f, "Initializing prefix...");

        cmd::Options opts;
        opts.env = baseEnv_;
        opts.env["WINEPREFIX"] = rootDir_;
        opts.env["WINEARCH"] = "win64";
        opts.env["WINEDEBUG"] = "-all";
        addLibPaths(opts);

        std::vector<std::string> fullArgs = wrapperArgs_;
        std::string binary;

        if (executorBinary_.empty()) {
            binary = resolveBinary("wineboot");
            fullArgs.push_back("-u");
        } else {
            binary = executorBinary_;
            fullArgs.insert(fullArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
            fullArgs.push_back("wineboot");
            fullArgs.push_back("-u");
        }

        cmd::Command::runSync(binary, fullArgs, opts);
    }

    if (cb) cb(1.0f, "Prefix ready.");
    return true;
}

bool Prefix::wine(const std::string& exe, const std::vector<std::string>& args,
                  std::function<void(const std::string&)> onOutput,
                  const std::string& cwd, bool wait,
                  const std::map<std::string, std::string>& extraEnv) {
    cmd::Options opts;
    opts.env = baseEnv_;
    for (const auto& [k, v] : extraEnv) opts.env[k] = v;
    
    opts.env["WINEPREFIX"] = rootDir_;
    opts.env["WINEARCH"] = "win64";
    if (!cwd.empty()) opts.cwd = cwd;
    
    addLibPaths(opts);

    std::vector<std::string> fullArgs = wrapperArgs_;
    std::string binary;

    if (executorBinary_.empty()) {
        binary = resolveBinary("wine");
        fullArgs.push_back(exe);
        fullArgs.insert(fullArgs.end(), args.begin(), args.end());
    } else {
        binary = executorBinary_;
        fullArgs.insert(fullArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
        fullArgs.push_back(exe);
        fullArgs.insert(fullArgs.end(), args.begin(), args.end());
    }

    if (!wait) {
        cmd::Command::runAsync(binary, fullArgs, opts, nullptr);
        return true;
    }

    stream_buffer_t buf;
    buf.connect([&](const std::string_view chunk) {
        if (onOutput) onOutput(std::string(chunk));
    });
    auto res = cmd::Command::runSync(binary, fullArgs, opts, onOutput ? &buf : nullptr);

    if (onOutput) onOutput(""); 

    return res.exitCode == 0;
}

bool Prefix::kill() {
    cmd::Options opts;
    opts.env = baseEnv_;
    opts.env["WINEPREFIX"] = rootDir_;
    
    std::vector<std::string> fullArgs = wrapperArgs_;
    std::string binary;

    if (executorBinary_.empty()) {
        binary = resolveBinary("wineserver");
        fullArgs.push_back("-k");
    } else {
        binary = executorBinary_;
        fullArgs.insert(fullArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
        fullArgs.push_back("wineserver");
        fullArgs.push_back("-k");
    }

    auto res = cmd::Command::runSync(binary, fullArgs, opts);
    return res.exitCode == 0;
}

void Prefix::registryAdd(const std::string& k, const std::string& n, const std::string& v, const std::string& t) {
    pendingReg_.push_back({k, n, v, t});
}

std::string Prefix::generateRegFile() const {
    std::stringstream ss;
    ss << "Windows Registry Editor Version 5.00\r\n\r\n";

    std::string lastKey;
    for (const auto& e : pendingReg_) {
        if (e.key != lastKey) {
            ss << "[" << e.key << "]\r\n";
            lastKey = e.key;
        }
        ss << (e.valueName.empty() ? "@" : "\"" + e.valueName + "\"") << "=";

        if (e.type == "REG_DWORD") {
            try {
                char buf[16];
                snprintf(buf, 16, "dword:%08lx", std::stoul(e.value, nullptr, 0));
                ss << buf;
            } catch (...) { ss << "dword:00000000"; }
        } else {
            std::string esc = e.value;
            size_t pos = 0;
            while ((pos = esc.find('\\', pos)) != std::string::npos) { esc.replace(pos, 1, "\\\\"); pos += 2; }
            pos = 0;
            while ((pos = esc.find('"', pos)) != std::string::npos) { esc.replace(pos, 1, "\\\""); pos += 2; }
            ss << "\"" << esc << "\"";
        }
        ss << "\r\n";
    }
    return ss.str();
}

bool Prefix::registryCommit() {
    if (pendingReg_.empty()) return true;

    auto regPath = fs::path(rootDir_) / "batch.reg";
    {
        std::ofstream ofs(regPath);
        ofs << generateRegFile();
    }

    cmd::Options opts;
    opts.env = baseEnv_;
    opts.env["WINEPREFIX"] = rootDir_;
    addLibPaths(opts);

    std::vector<std::string> fullArgs = wrapperArgs_;
    std::string binary;

    if (executorBinary_.empty()) {
        binary = resolveBinary("wine");
        fullArgs.push_back("regedit.exe");
        fullArgs.push_back("/s");
        fullArgs.push_back("Z:" + regPath.string());
    } else {
        binary = executorBinary_;
        fullArgs.insert(fullArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
        fullArgs.push_back("regedit.exe");
        fullArgs.push_back("/s");
        fullArgs.push_back("Z:" + regPath.string());
    }

    auto res = cmd::Command::runSync(binary, fullArgs, opts);

    fs::remove(regPath);
    if (res.exitCode == 0) pendingReg_.clear();
    return res.exitCode == 0;
}

std::optional<std::string> Prefix::registryQuery(const std::string& key, const std::string& name) {
    cmd::Options opts;
    opts.env = baseEnv_;
    opts.env["WINEPREFIX"] = rootDir_;
    opts.mergeStdoutStderr = true;
    addLibPaths(opts);

    std::vector<std::string> fullArgs = wrapperArgs_;
    std::string binary;

    if (executorBinary_.empty()) {
        binary = resolveBinary("wine");
        fullArgs.push_back("reg");
        fullArgs.push_back("query");
        fullArgs.push_back(key);
        fullArgs.push_back("/v");
        fullArgs.push_back(name);
    } else {
        binary = executorBinary_;
        fullArgs.insert(fullArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
        fullArgs.push_back("reg");
        fullArgs.push_back("query");
        fullArgs.push_back(key);
        fullArgs.push_back("/v");
        fullArgs.push_back(name);
    }

    stream_buffer_t buf;
    auto res = cmd::Command::runSync(binary, fullArgs, opts, &buf);

    if (res.exitCode != 0) return std::nullopt;

    std::string output = buf.view();
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find(name) != std::string::npos) {
            size_t typePos = line.find("REG_");
            if (typePos != std::string::npos) {
                size_t valStart = line.find_first_not_of(" \t", typePos + 6);
                if (valStart != std::string::npos) {
                    std::string val = line.substr(valStart);
                    val.erase(val.find_last_not_of(" \t\r\n") + 1);
                    return val;
                }
            }
        }
    }

    return std::nullopt;
}

bool Prefix::installDxvk(const std::string& dxvkRoot) {
    fs::path root(dxvkRoot);
    if (!fs::exists(root)) return false;

    fs::path sys32 = fs::path(rootDir_) / "drive_c" / "windows" / "system32";
    fs::path syswow = fs::path(rootDir_) / "drive_c" / "windows" / "syswow64";

    auto installDlls = [&](fs::path src, fs::path dst) {
        if (!fs::exists(src) || !fs::exists(dst)) return;
        for (const auto& entry : fs::directory_iterator(src)) {
            if (entry.path().extension() == ".dll") {
                fs::path target = dst / entry.path().filename();
                try {
                    if (fs::exists(target)) fs::remove(target);
                    fs::copy_file(entry.path(), target);
                    registryAdd("HKCU\\Software\\Wine\\DllOverrides", 
                                entry.path().stem().string(), 
                                "native", "REG_SZ");
                } catch(...) {}
            }
        }
    };

    if (fs::exists(root / "x64")) {
        installDlls(root / "x64", sys32);
        installDlls(root / "x32", syswow);
    } else if (fs::exists(root / "x86_64-windows")) {
        installDlls(root / "x86_64-windows", sys32);
        installDlls(root / "i386-windows", syswow);
    } else if (fs::exists(root / "files/lib/wine/dxvk/x86_64-windows")) {
        installDlls(root / "files/lib/wine/dxvk/x86_64-windows", sys32);
        installDlls(root / "files/lib/wine/dxvk/i386-windows", syswow);
    }
    
    return registryCommit();
}

}
