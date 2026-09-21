#pragma once
#include "Bank.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace phial::blacklist {
using Rules = std::map<std::string, std::string>;
inline std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string hex(const storage::Bytes& b) {
    constexpr char digits[] = "0123456789abcdef";
    std::string s; s.reserve(b.size()*2);
    for (auto c : b) { s += digits[c>>4]; s += digits[c&15]; } return s;
}
inline std::string staticKey(storage::Ref r) {
    if (r.file.empty() || !r.local) throw storage::Error("Missing persistent identity");
    storage::Writer w; r.file = lower(r.file); w.ref(r); return "S:" + hex(w.data);
}
inline std::string recipeKey(bool poison, const std::vector<storage::Effect>& effects) {
    if (effects.empty() || effects.size()>128) throw storage::Error("Invalid recipe");
    std::vector<storage::Bytes> parts;
    for (auto e : effects) {
        if (e.base.file.empty() || !e.base.local) throw storage::Error("Missing effect identity");
        storage::Writer w; e.base.file=lower(e.base.file); w.ref(e.base);
        w.f32(e.magnitude == 0 ? 0 : e.magnitude); w.u32(e.area); w.u32(e.duration);
        parts.push_back(std::move(w.data));
    }
    std::sort(parts.begin(),parts.end());
    storage::Writer w; w.u32(poison); w.u32(static_cast<std::uint32_t>(parts.size()));
    for (const auto& p : parts) w.data.insert(w.data.end(),p.begin(),p.end());
    return "R:" + hex(w.data);
}
inline void validate(const Rules& rules) {
    if (rules.size()>4096) throw storage::Error("Blacklist is full (4096 entries)");
    std::size_t total=0;
    for (const auto& [key,label] : rules) {
        if (key.size()<4 || key.size()>80000 || (key.substr(0,2)!="R:" && key.substr(0,2)!="S:") ||
            key.find_first_not_of("0123456789abcdef",2)!=key.npos || (key.size()%2)!=0 ||
            label.size()>4096 || label.find('\0')!=label.npos)
            throw storage::Error("Invalid blacklist entry");
        total+=key.size()+label.size()*2+8;
    }
    if (total>16*1024*1024) throw storage::Error("Blacklist exceeds size limit");
}
inline std::string encode(const Rules& rules) {
    validate(rules); std::ostringstream out; out << "WhitePhialBlacklist 1\n";
    for (const auto& [key,label] : rules) out << std::quoted(key) << ' ' << std::quoted(label) << '\n';
    return out.str();
}
inline Rules decode(const std::string& text) {
    if (text.size()>16*1024*1024) throw storage::Error("Blacklist file too large");
    std::istringstream in(text); std::string header; std::getline(in,header);
    if (header!="WhitePhialBlacklist 1") throw storage::Error("Unknown blacklist format");
    Rules rules;
    while (in >> std::ws && !in.eof()) {
        std::string key,label;
        if (!(in >> std::quoted(key) >> std::quoted(label)) || !rules.emplace(key,label).second)
            throw storage::Error("Malformed blacklist");
    }
    validate(rules); return rules;
}
inline Rules load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    auto size=std::filesystem::file_size(path);
    if (size>16*1024*1024) throw storage::Error("Blacklist file too large");
    std::ifstream in(path,std::ios::binary); std::string text(static_cast<std::size_t>(size),'\0');
    if (!in.read(text.data(),static_cast<std::streamsize>(size))) throw storage::Error("Cannot read blacklist");
    return decode(text);
}
inline void save(const std::filesystem::path& path,const Rules& rules) {
    const auto text=encode(rules);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto tmp=path; tmp += ".tmp";
    { std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
      out.write(text.data(),static_cast<std::streamsize>(text.size())); out.close();
      if (!out) throw storage::Error("Cannot write blacklist temporary file"); }
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw storage::Error("Cannot replace blacklist file; old rules retained");
#else
    std::filesystem::rename(tmp,path);
#endif
}
}
