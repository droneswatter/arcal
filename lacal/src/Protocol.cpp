#include "Protocol.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace arcal::lacal {

std::string trim(std::string s) {
    auto isWs = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

bool validToken(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-' || c == '.';
    });
}

bool globMatch(const std::string& pattern, const std::string& text) {
    std::size_t p = 0, t = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

bool headerContainsToken(const std::string& value, const std::string& token) {
    std::istringstream in(value);
    std::string part;
    while (std::getline(in, part, ',')) {
        if (trim(part) == token) return true;
    }
    return false;
}

std::optional<std::string> firstField(const std::string& line, std::string& rest) {
    std::string s = trim(line);
    if (s.empty()) return std::nullopt;
    auto pos = s.find(' ');
    if (pos == std::string::npos) {
        rest.clear();
        return s;
    }
    rest = trim(s.substr(pos + 1));
    return s.substr(0, pos);
}

std::vector<std::string> splitFields(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string field;
    while (in >> field) out.push_back(field);
    return out;
}

std::string makeInfoJson(const std::string& serviceId, uint16_t port) {
    nlohmann::json info;
    info["version"] = "1.0";
    info["server_id"] = "arlacal-server";
    info["system_label"] = "arcal";
    info["service_id"] = serviceId;
    info["connect_urls"] = {std::string("ws://127.0.0.1:") + std::to_string(port)};
    info["extensions"] = {"arcal.xsub.v1"};
    info["uuids"] = {
        {"system", "00000000-0000-0000-0000-000000000000"},
        {"service", "00000000-0000-0000-0000-000000000000"}
    };
    return info.dump();
}

} // namespace arcal::lacal
