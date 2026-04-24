#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arcal::lacal {

std::string trim(std::string s);
bool validToken(const std::string& s);
bool globMatch(const std::string& pattern, const std::string& text);
bool headerContainsToken(const std::string& value, const std::string& token);
std::optional<std::string> firstField(const std::string& line, std::string& rest);
std::vector<std::string> splitFields(const std::string& s);
std::string makeInfoJson(const std::string& serviceId, uint16_t port);

} // namespace arcal::lacal
