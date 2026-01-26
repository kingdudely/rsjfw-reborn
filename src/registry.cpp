#include "registry.h"
#include "logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <strings.h>
#include <thread>

namespace rsjfw {

namespace fs = std::filesystem;

static std::string decodeW(const std::vector<uint8_t> &b) {
  if (b.size() < 2)
    return "";
  std::string res;
  for (size_t i = 0; i + 1 < b.size(); i += 2) {
    uint16_t val = b[i] | (static_cast<uint16_t>(b[i + 1]) << 8);
    if (val == 0)
      break;
    if (val < 128)
      res += static_cast<char>(val);
    else
      res += '?';
  }
  return res;
}

static std::vector<uint8_t> to_utf16(const std::string &s) {
  std::vector<uint8_t> out;
  for (char c : s) {
    out.push_back(static_cast<uint8_t>(c));
    out.push_back(0);
  }
  out.push_back(0);
  out.push_back(0);
  return out;
}

static std::string to_hex_wrapped(const std::vector<uint8_t> &data,
                                  size_t startCol) {
  std::stringstream ss;
  size_t col = startCol;
  for (size_t i = 0; i < data.size(); ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(data[i]);
    if (i < data.size() - 1) {
      ss << ",";
      col += 3;
      if (col >= 75) {
        ss << "\\\n  ";
        col = 2;
      }
    }
  }
  return ss.str();
}

static std::vector<uint8_t> parseBytes(const std::string &s) {
  std::vector<uint8_t> out;
  std::stringstream ss(s);
  std::string b;
  while (std::getline(ss, b, ',')) {
    size_t st = b.find_first_not_of(" \t\r\n");
    if (st == std::string::npos)
      continue;
    if (b[st] == '\\')
      break;
    try {
      out.push_back(
          static_cast<uint8_t>(std::stoul(b.substr(st), nullptr, 16)));
    } catch (...) {
    }
  }
  return out;
}

std::string RegistryValue::asString() const {
  if (type == RegistryType::String || type == RegistryType::ExpandString ||
      type == RegistryType::Link) {
    std::string s(data.begin(), data.end());
    while (!s.empty() && s.back() == 0)
      s.pop_back();
    return s;
  }
  if (type == RegistryType::Dword) {
    char buf[32];
    snprintf(buf, sizeof(buf), "dword:%08x", asDword());
    return buf;
  }
  return "hex:" + to_hex_wrapped(data, 4);
}

uint32_t RegistryValue::asDword() const {
  uint32_t v = 0;
  if (data.size() >= 4)
    std::memcpy(&v, data.data(), 4);
  return v;
}

std::vector<std::string> RegistryValue::asMultiString() const {
  std::vector<std::string> out;
  std::string cur;
  for (uint8_t b : data) {
    if (b == 0) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else
      cur += static_cast<char>(b);
  }
  return out;
}

RegistryValue *RegistryKey::getValue(const std::string &name) {
  for (auto &v : values)
    if (strcasecmp(v.name.c_str(), name.c_str()) == 0)
      return &v;
  return nullptr;
}

void RegistryKey::setValue(const std::string &name, RegistryType type,
                           const std::vector<uint8_t> &data,
                           uint32_t customType) {
  wasInFile = true;
  if (auto *v = getValue(name)) {
    v->type = type;
    v->data = data;
    v->customType = customType;
    return;
  }
  values.push_back({name, type, data, customType});
}

bool RegistryKey::deleteValue(const std::string &name) {
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (strcasecmp(it->name.c_str(), name.c_str()) == 0) {
      values.erase(it);
      wasInFile = true;
      return true;
    }
  }
  return false;
}

RegistryKey *RegistryKey::add(const std::string &path) {
  return queryPath(path, true);
}
RegistryKey *RegistryKey::query(const std::string &path) {
  return queryPath(path, false);
}

RegistryKey *RegistryKey::queryPath(const std::string &path, bool create) {
  if (path.empty())
    return this;
  std::string seg;
  std::stringstream ss(path);
  RegistryKey *cur = this;
  while (std::getline(ss, seg, '\\')) {
    if (seg.empty())
      continue;
    RegistryKey *next = nullptr;
    for (auto &sk : cur->subkeys) {
      if (strcasecmp(sk->name.c_str(), seg.c_str()) == 0) {
        next = sk.get();
        break;
      }
    }
    if (!next) {
      if (create) {
        auto n = std::make_shared<RegistryKey>(seg, cur);
        cur->subkeys.push_back(n);
        next = n.get();
      } else
        return nullptr;
    }
    cur = next;
  }
  return cur;
}

RegistryKey *RegistryKey::root() {
  RegistryKey *cur = this;
  while (cur->parent)
    cur = cur->parent;
  return cur;
}

bool RegistryKey::deleteKey(const std::string &path) {
  RegistryKey *k = query(path);
  if (!k || !k->parent)
    return false;
  for (auto it = k->parent->subkeys.begin(); it != k->parent->subkeys.end();
       ++it) {
    if (it->get() == k) {
      k->parent->subkeys.erase(it);
      return true;
    }
  }
  return false;
}

void RegistryKey::copyFrom(const RegistryKey &other) {
  values = other.values;
  modified = other.modified;
  isLink = other.isLink;
  unixTime = other.unixTime;
  wasInFile = true;

  subkeys.clear();
  for (const auto &sk : other.subkeys) {
    auto n = std::make_shared<RegistryKey>(sk->name, this);
    n->copyFrom(*sk);
    subkeys.push_back(n);
  }
}

std::string RegistryKey::unescape(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.length(); ++i) {
    if (s[i] == '\\' && i + 1 < s.length()) {
      if (s[i + 1] == '\\' || s[i + 1] == '"') {
        out += s[i + 1];
        i++;
      } else
        out += '\\';
    } else
      out += s[i];
  }
  return out;
}

std::string RegistryKey::escape(const std::string &s) {
  std::string out;
  for (unsigned char c : s) {
    if (c == '\\')
      out += "\\\\";
    else if (c == '"')
      out += "\\\"";
    else
      out += c;
  }
  return out;
}

std::string RegistryKey::unquote(const std::string &s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return unescape(s.substr(1, s.size() - 2));
  return unescape(s);
}

std::optional<RegistryValue> RegistryKey::parseData(const std::string &val) {
  if (val.empty())
    return RegistryValue{"", RegistryType::String, {}};
  if (val[0] == '"') {
    std::string s = unquote(val);
    return RegistryValue{"", RegistryType::String, {s.begin(), s.end()}};
  }
  if (val == "-")
    return std::nullopt;
  size_t col = val.find(':');
  if (col == std::string::npos)
    return std::nullopt;
  std::string pref = val.substr(0, col), dat = val.substr(col + 1);
  try {
    if (pref == "dword") {
      uint32_t v = std::stoul(dat, nullptr, 16);
      std::vector<uint8_t> b(4);
      std::memcpy(b.data(), &v, 4);
      return RegistryValue{"", RegistryType::Dword, b};
    }
    if (pref.find("hex") == 0) {
      std::vector<uint8_t> hex = parseBytes(dat);
      RegistryType t = RegistryType::Binary;
      std::vector<uint8_t> finalData = hex;
      uint32_t cType = 0;
      if (pref == "hex")
        t = RegistryType::Binary;
      else if (pref.size() > 4 && pref[3] == '(' && pref.back() == ')') {
        cType = std::stoul(pref.substr(4, pref.size() - 5), nullptr, 16);
        if (cType == 1)
          t = RegistryType::String;
        else if (cType == 2) {
          t = RegistryType::ExpandString;
          std::string s = decodeW(hex);
          finalData.assign(s.begin(), s.end());
        } else if (cType == 6) {
          t = RegistryType::Link;
          std::string s = decodeW(hex);
          finalData.assign(s.begin(), s.end());
        } else if (cType == 7) {
          t = RegistryType::MultiString;
          std::string s = decodeW(hex);
          finalData.assign(s.begin(), s.end());
        } else if (cType == 11)
          t = RegistryType::Qword;
        else
          t = RegistryType::Custom;
      }
      auto rv = RegistryValue{"", t, finalData};
      if (t == RegistryType::Custom)
        rv.customType = cType;
      return rv;
    }
    if (pref == "str(2)") {
      std::string s = unquote(dat);
      return RegistryValue{
          "", RegistryType::ExpandString, {s.begin(), s.end()}};
    }
    if (pref == "str(7)") {
      std::string s = unquote(dat);
      return RegistryValue{"", RegistryType::MultiString, {s.begin(), s.end()}};
    }
  } catch (...) {
  }
  return std::nullopt;
}

bool RegistryKey::load(std::istream &is) {
  std::string line;
  if (!std::getline(is, line))
    return false;
  RegistryKey *sub = nullptr;
  while (std::getline(is, line)) {
    if (line.empty())
      continue;

    while (!line.empty()) {
      size_t last = line.find_last_not_of(" \t\r\n");
      if (last != std::string::npos && line[last] == '\\') {
        line.erase(last);
        std::string next;
        if (std::getline(is, next)) {
          size_t first = next.find_first_not_of(" \t");
          if (first != std::string::npos)
            line += next.substr(first);
        } else
          break;
      } else
        break;
    }

    size_t st = line.find_first_not_of(" \t");
    if (st == std::string::npos)
      continue;
    char f = line[st];
    if (f == ';')
      continue;
    if (f == '#') {
      if (line.substr(st).find("#time=") == 0) {
        try {
          uint64_t t = std::stoull(line.substr(st + 6), nullptr, 16);
          if (sub)
            sub->modified = t;
          else
            this->modified = t;
        } catch (...) {
        }
      } else if (line.substr(st).find("#link") == 0) {
        if (sub)
          sub->isLink = true;
      }
      continue;
    }
    if (f == '[') {
      size_t end = line.find(']', st);
      if (end == std::string::npos)
        continue;
      std::string p = unescape(line.substr(st + 1, end - st - 1));
      if (p.find("-") == 0) {
        deleteKey(p.substr(1));
        sub = nullptr;
      } else {
        sub = add(p);
        RegistryKey *temp = sub;
        while (temp) {
          temp->wasInFile = true;
          temp = temp->parent;
        }
        try {
          size_t ts_pos = line.find(' ', end);
          if (ts_pos != std::string::npos)
            sub->unixTime = std::stoul(line.substr(ts_pos + 1));
        } catch (...) {
        }
      }
      continue;
    }
    if (f == '"' || f == '@') {
      if (!sub)
        continue;
      size_t eq = line.find('=', st);
      if (eq == std::string::npos)
        continue;
      std::string vn = line.substr(st, eq - st);
      if (vn == "@")
        vn = "";
      else if (vn.size() >= 2 && vn[0] == '"')
        vn = unquote(vn);
      std::string dataPart = line.substr(eq + 1);
      if (dataPart == "-") {
        sub->deleteValue(vn);
      } else {
        auto v = parseData(dataPart);
        if (v) {
          sub->setValue(vn, v->type, v->data, v->customType);
          sub->wasInFile = true;
        }
      }
    }
  }
  return true;
}

bool RegistryKey::save(std::ostream &os, const std::string &root) {
  bool hasSomething = !values.empty() || modified || isLink || wasInFile;
  if (hasSomething && !root.empty()) {
    os << "[" << escape(root) << "] " << (unixTime ? unixTime : 1700000000)
       << "\n";
    if (modified)
      os << "#time=" << std::hex << modified << std::dec << "\n";
    if (isLink)
      os << "#link\n";
    for (const auto &v : values) {
      if (v.name.empty())
        os << "@=";
      else
        os << "\"" << escape(v.name) << "\"=";
      std::string s(v.data.begin(), v.data.end());
      while (!s.empty() && s.back() == 0)
        s.pop_back();
      switch (v.type) {
      case RegistryType::String:
        os << "\"" << escape(s) << "\"\n";
        break;
      case RegistryType::Dword:
        os << "dword:" << std::hex << std::setw(8) << std::setfill('0')
           << v.asDword() << std::dec << "\n";
        break;
      case RegistryType::ExpandString:
        os << "str(2):\"" << escape(s) << "\"\n";
        break;
      case RegistryType::MultiString:
        os << "str(7):\"" << escape(s) << "\"\n";
        break;
      case RegistryType::Link:
        os << "hex(6):" << to_hex_wrapped(to_utf16(s), 7) << "\n";
        break;
      case RegistryType::Binary:
        os << "hex:" << to_hex_wrapped(v.data, 4) << "\n";
        break;
      default:
        os << "hex(" << std::hex
           << (v.type == RegistryType::Custom ? v.customType
                                              : static_cast<int>(v.type))
           << "):" << to_hex_wrapped(v.data, 10) << std::dec << "\n";
        break;
      }
    }
    os << "\n";
  }
  std::vector<std::shared_ptr<RegistryKey>> sorted = subkeys;
  std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
    return strcasecmp(a->name.c_str(), b->name.c_str()) < 0;
  });
  for (auto &sk : sorted)
    sk->save(os, root.empty() ? sk->name : root + "\\" + sk->name);
  return true;
}

Registry::Registry(const std::string &p)
    : prefixDir_(p), lastSystem_(std::filesystem::file_time_type::min()),
      lastUser_(std::filesystem::file_time_type::min()) {
  loadHive("system.reg", machine_);
  loadHive("user.reg", currentUser_);
}

Registry::~Registry() { commit(); }

void Registry::checkAndReload() {
  auto ps = fs::path(prefixDir_) / "system.reg";
  auto pu = fs::path(prefixDir_) / "user.reg";
  if (fs::exists(ps)) {
    auto m = fs::last_write_time(ps);
    if (m > lastSystem_) {
      loadHive("system.reg", machine_);
      lastSystem_ = m;
    }
  }
  if (fs::exists(pu)) {
    auto m = fs::last_write_time(pu);
    if (m > lastUser_) {
      loadHive("user.reg", currentUser_);
      lastUser_ = m;
    }
  }
}

bool Registry::loadHive(const std::string &f, std::shared_ptr<RegistryKey> &k) {
  fs::path p = fs::path(prefixDir_) / f;
  if (!fs::exists(p))
    return false;
  std::ifstream is(p);
  if (!is.is_open())
    return false;
  std::string firstLine;
  if (std::getline(is, firstLine)) {
    std::string secondLine;
    if (std::getline(is, secondLine) &&
        secondLine.find(";; All keys relative to ") == 0) {
      std::string rel = secondLine.substr(24);
      if (f == "system.reg")
        systemRelativePath_ = rel;
      else
        userRelativePath_ = rel;
    }
    is.seekg(0);
    auto nk = std::make_shared<RegistryKey>();
    if (nk->load(is)) {
      k = nk;
      LOG_INFO("Loaded hive %s: %zu subkeys", f.c_str(), k->subkeys.size());
      return true;
    }
  }
  return false;
}

bool Registry::saveHive(const std::string &f, std::shared_ptr<RegistryKey> &k,
                        const std::string &r) {
  if (!k)
    return true;
  fs::path p = fs::path(prefixDir_) / f;
  fs::path p_tmp = p;
  p_tmp += ".tmp";
  std::ofstream os(p_tmp, std::ios::trunc);
  if (!os.is_open())
    return false;
  os << "WINE REGISTRY Version 2\n;; All keys relative to " << r
     << "\n\n#arch=win64\n\n";
  k->save(os, "");
  os.close();
  try {
    fs::rename(p_tmp, p);
  } catch (...) {
    return false;
  }
  if (f == "system.reg")
    lastSystem_ = fs::last_write_time(p);
  else
    lastUser_ = fs::last_write_time(p);
  return true;
}

RegistryKey *Registry::getRoot(const std::string &p, std::string &s) {
  size_t i = p.find('\\');
  std::string r = (i == std::string::npos) ? p : p.substr(0, i);
  s = (i == std::string::npos) ? "" : p.substr(i + 1);
  std::transform(r.begin(), r.end(), r.begin(), ::toupper);
  if (r == "HKLM" || r == "HKEY_LOCAL_MACHINE") {
    if (!machine_)
      machine_ = std::make_shared<RegistryKey>("HKLM");
    return machine_.get();
  }
  if (r == "HKCU" || r == "HKEY_CURRENT_USER") {
    if (!currentUser_)
      currentUser_ = std::make_shared<RegistryKey>("HKCU");
    return currentUser_.get();
  }
  return nullptr;
}

std::optional<std::string> Registry::query(const std::string &p,
                                           const std::string &n) {
  std::unique_lock l(mutex_);
  checkAndReload();
  std::string s;
  RegistryKey *r = getRoot(p, s);
  if (r) {
    if (auto *k = r->query(s)) {
      if (auto *v = k->getValue(n))
        return v->asString();
    }
  }
  return std::nullopt;
}

void Registry::add(const std::string &p, const std::string &n,
                   const std::string &v, const std::string &t) {
  std::unique_lock l(mutex_);
  checkAndReload();
  std::string s;
  RegistryKey *r = getRoot(p, s);
  if (!r)
    return;
  RegistryKey *k = r->add(s);
  RegistryType rt = RegistryType::String;
  std::vector<uint8_t> d;
  if (t == "REG_DWORD") {
    rt = RegistryType::Dword;
    uint32_t iv = std::stoul(v, nullptr, 0);
    d.resize(4);
    std::memcpy(d.data(), &iv, 4);
  } else if (t == "REG_BINARY") {
    rt = RegistryType::Binary;
    d = parseBytes(v);
  } else {
    d.assign(v.begin(), v.end());
  }
  k->setValue(n, rt, d);
}

void Registry::transplant(const std::string &path, Registry &source) {
  std::unique_lock l1(mutex_);
  std::unique_lock l2(source.mutex_);
  checkAndReload();
  source.checkAndReload();

  std::string s;
  RegistryKey *rDest = getRoot(path, s);
  RegistryKey *rSrc = source.getRoot(path, s);

  if (!rDest || !rSrc)
    return;

  RegistryKey *kSrc = rSrc->query(s);
  if (!kSrc)
    return;

  RegistryKey *kDest = rDest->add(s);
  kDest->copyFrom(*kSrc);
}

bool Registry::commit() {
  std::unique_lock l(mutex_);
  saveHive("system.reg", machine_, systemRelativePath_);
  saveHive("user.reg", currentUser_, userRelativePath_);
  return true;
}

} // namespace rsjfw
