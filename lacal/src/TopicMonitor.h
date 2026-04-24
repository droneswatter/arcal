#pragma once

#include "ClientSession.h"

#include <dds/dds.h>

#include <atomic>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace uci::base {
class Externalizer;
} // namespace uci::base

namespace arcal::lacal {

class TopicMonitor {
public:
    TopicMonitor(ClientRegistry& clients, uci::base::Externalizer& jsonExt, int domainId);
    ~TopicMonitor();

    TopicMonitor(const TopicMonitor&) = delete;
    TopicMonitor& operator=(const TopicMonitor&) = delete;

    void start();
    void stop();

private:
    void run();
    void pollBuiltin();
    void ensureReader(const std::string& topicName);
    void drainReader(const std::string& topicName);
    void publishDecoded(const std::string& topicName, uint32_t tag,
                        const std::vector<uint8_t>& data);
    void deliver(const std::string& topicName,
                 const std::string& messageName,
                 const std::string& json);

    ClientRegistry& clients_;
    uci::base::Externalizer& jsonExt_;
    dds_entity_t participant_ = -1;
    dds_entity_t builtinReader_ = -1;
    dds_entity_t builtinCond_ = -1;
    dds_entity_t waitset_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::unordered_map<std::string, dds_entity_t> readers_;
    std::unordered_map<std::string, dds_entity_t> conditions_;
    std::unordered_map<dds_entity_t, std::string> condTopics_;
};

} // namespace arcal::lacal
