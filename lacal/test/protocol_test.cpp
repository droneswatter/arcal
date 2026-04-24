#include "Protocol.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool cond, const char* what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    using namespace arcal::lacal;

    require(trim(" \t INIT {}\r\n") == "INIT {}", "trim removes OWP line whitespace");
    require(validToken("sub-1.alpha_beta"), "valid token accepts common subscription ids");
    require(!validToken("bad token"), "valid token rejects spaces");
    require(!validToken(""), "valid token rejects empty strings");

    std::string rest;
    auto op = firstField("  SUB sub1 ActionCommandMT ActionCommand  ", rest);
    require(op && *op == "SUB", "firstField extracts operation");
    require(rest == "sub1 ActionCommandMT ActionCommand", "firstField trims rest");
    require(!firstField(" \t ", rest), "firstField ignores empty lines");

    const std::vector<std::string> fields = splitFields("sub1 ActionCommandMT ActionCommand groupA");
    require(fields.size() == 4, "splitFields returns expected field count");
    require(fields[2] == "ActionCommand", "splitFields preserves field text");

    require(globMatch("*", "PositionReport"), "glob star matches everything");
    require(globMatch("System*", "SystemStatus"), "glob prefix match");
    require(globMatch("Service?Status", "Service1Status"), "glob question mark match");
    require(!globMatch("Service?Status", "Service12Status"), "glob question mark is single char");

    require(headerContainsToken("chat, owp, superchat", "owp"), "header token match");
    require(!headerContainsToken("not-owp, nope", "owp"), "header token does not substring match");

    const auto info = nlohmann::json::parse(makeInfoJson("svc", 8766));
    require(info["version"] == "1.0", "INFO version");
    require(info["service_id"] == "svc", "INFO service id");
    require(info["extensions"].at(0) == "arcal.xsub.v1", "INFO extension");
    require(info["connect_urls"].at(0) == "ws://127.0.0.1:8766", "INFO connect url");

    std::cout << "PASS lacal_protocol_test\n";
    return 0;
}
