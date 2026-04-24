#include "TopicMonitor.h"

#include "Protocol.h"
#include "arcal/AccessorFactory.h"
#include "arcal/CdrBridge.h"
#include "arcal_payload.h"
#include "uci/base/Accessor.h"
#include "uci/base/Externalizer.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace arcal::lacal {
namespace {

constexpr const char* kArcalTypeName = "arcal_dds::OpaquePayload";

} // namespace

TopicMonitor::TopicMonitor(ClientRegistry& clients, uci::base::Externalizer& jsonExt, int domainId)
    : clients_(clients), jsonExt_(jsonExt) {
    participant_ = dds_create_participant(static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
    if (participant_ < 0)
        throw std::runtime_error("failed to create DDS participant");

    builtinReader_ = dds_create_reader(participant_, DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
                                       nullptr, nullptr);
    if (builtinReader_ < 0)
        throw std::runtime_error("failed to create DDS builtin publication reader");

    builtinCond_ = dds_create_readcondition(builtinReader_, DDS_ANY_STATE);
    waitset_ = dds_create_waitset(participant_);
    dds_waitset_attach(waitset_, builtinCond_, static_cast<dds_attach_t>(builtinCond_));
}

TopicMonitor::~TopicMonitor() {
    stop();
    if (thread_.joinable()) thread_.join();
    for (auto& [_, cond] : conditions_) dds_delete(cond);
    for (auto& [_, reader] : readers_) dds_delete(reader);
    if (waitset_ >= 0) dds_delete(waitset_);
    if (builtinCond_ >= 0) dds_delete(builtinCond_);
    if (builtinReader_ >= 0) dds_delete(builtinReader_);
    if (participant_ >= 0) dds_delete(participant_);
}

void TopicMonitor::start() {
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void TopicMonitor::stop() {
    running_ = false;
}

void TopicMonitor::run() {
    constexpr int kMaxTriggers = 64;
    dds_attach_t triggered[kMaxTriggers];
    while (running_) {
        int n = dds_waitset_wait(waitset_, triggered, kMaxTriggers, DDS_MSECS(500));
        if (n < 0) break;
        for (int i = 0; i < n; ++i) {
            auto cond = static_cast<dds_entity_t>(triggered[i]);
            if (cond == builtinCond_) {
                pollBuiltin();
            } else {
                auto it = condTopics_.find(cond);
                if (it != condTopics_.end()) drainReader(it->second);
            }
        }
    }
}

void TopicMonitor::pollBuiltin() {
    static constexpr int kBatch = 32;
    void* ptrs[kBatch] = {};
    dds_sample_info_t info[kBatch];
    int n = dds_take(builtinReader_, ptrs, info, kBatch, kBatch);
    for (int i = 0; i < n; ++i) {
        if (!info[i].valid_data) continue;
        auto* ep = static_cast<dds_builtintopic_endpoint_t*>(ptrs[i]);
        if (ep->topic_name && ep->type_name &&
            std::strcmp(ep->type_name, kArcalTypeName) == 0) {
            ensureReader(ep->topic_name);
        }
    }
    dds_return_loan(builtinReader_, ptrs, n);
}

void TopicMonitor::ensureReader(const std::string& topicName) {
    if (readers_.count(topicName)) return;

    dds_entity_t topic = dds_create_topic(participant_, &arcal_dds_OpaquePayload_desc,
                                          topicName.c_str(), nullptr, nullptr);
    if (topic < 0) {
        std::cerr << "[arlacal] failed to create topic reader for " << topicName << "\n";
        return;
    }

    dds_entity_t reader = dds_create_reader(participant_, topic, nullptr, nullptr);
    dds_delete(topic);
    if (reader < 0) {
        std::cerr << "[arlacal] failed to create reader for " << topicName << "\n";
        return;
    }

    dds_entity_t cond = dds_create_readcondition(reader, DDS_ANY_STATE);
    dds_waitset_attach(waitset_, cond, static_cast<dds_attach_t>(cond));
    readers_[topicName] = reader;
    conditions_[topicName] = cond;
    condTopics_[cond] = topicName;
    std::cout << "[arlacal] discovered topic=" << topicName << "\n";
}

void TopicMonitor::drainReader(const std::string& topicName) {
    auto readerIt = readers_.find(topicName);
    if (readerIt == readers_.end()) return;

    static constexpr int kBatch = 16;
    void* ptrs[kBatch] = {};
    dds_sample_info_t info[kBatch];
    int n = dds_take(readerIt->second, ptrs, info, kBatch, kBatch);
    for (int i = 0; i < n; ++i) {
        if (!info[i].valid_data) continue;
        auto* sample = static_cast<arcal_dds_OpaquePayload*>(ptrs[i]);
        std::vector<uint8_t> data(sample->data._buffer,
                                  sample->data._buffer + sample->data._length);
        publishDecoded(topicName, sample->type_tag, data);
    }
    dds_return_loan(readerIt->second, ptrs, n);
}

void TopicMonitor::publishDecoded(const std::string& topicName, uint32_t tag,
                                  const std::vector<uint8_t>& data) {
    uci::base::Accessor* raw = arcal::arcalCreateAccessor(tag);
    if (!raw) {
        std::cerr << "[arlacal] unknown type tag 0x" << std::hex << tag
                  << std::dec << " topic=" << topicName << "\n";
        return;
    }
    std::unique_ptr<uci::base::Accessor, void(*)(uci::base::Accessor*)>
        acc(raw, arcal::arcalDestroyAccessor);

    try {
        arcal::cdrDeserialize(tag, data, *acc);
        std::string json;
        jsonExt_.write(*acc, json);
        const std::string messageName = acc->typeName();
        deliver(topicName, messageName, json);
    } catch (const std::exception& e) {
        std::cerr << "[arlacal] decode failed topic=" << topicName
                  << " tag=0x" << std::hex << tag << std::dec
                  << ": " << e.what() << "\n";
    }
}

void TopicMonitor::deliver(const std::string& topicName,
                           const std::string& messageName,
                           const std::string& json) {
    for (auto& client : clients_.snapshot()) {
        std::vector<XSub> xsubs;
        std::vector<StandardSub> standardSubs;
        {
            std::lock_guard<std::mutex> lock(client->stateMutex);
            xsubs = client->xsubs;
            for (const auto& [_, sub] : client->standardSubs)
                standardSubs.push_back(sub);
        }

        for (const StandardSub& sub : standardSubs) {
            if (sub.topic == topicName && sub.messageName == messageName)
                client->sendLine("MSG " + sub.subscriptionId + " " + json);
        }

        for (const XSub& xsub : xsubs) {
            if (!globMatch(xsub.topicPattern, topicName) ||
                !globMatch(xsub.messagePattern, messageName)) {
                continue;
            }

            std::string subId;
            const bool isNew = client->ensureConcreteSubscription(
                xsub.streamId, topicName, messageName, subId);
            if (isNew) {
                client->sendLine("XSUBINFO " + xsub.streamId + " " + subId + " " +
                                 topicName + " " + messageName);
            }
            client->sendLine("MSG " + subId + " " + json);
        }
    }
    clients_.removeClosed();
}

} // namespace arcal::lacal
