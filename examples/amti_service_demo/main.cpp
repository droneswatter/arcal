#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/UUID.h"
#include "uci/type/AMTI_ActivityMT.h"
#include "uci/type/AMTI_ActivityType.h"
#include "uci/type/AMTI_CapabilityMT.h"
#include "uci/type/AMTI_CapabilityStatusMT.h"
#include "uci/type/AMTI_CapabilityType.h"
#include "uci/type/AMTI_CommandMT.h"
#include "uci/type/AMTI_CommandStatusMT.h"
#include "uci/type/AMTI_CommandType.h"
#include "uci/type/AMTI_MessageOutputsEnum.h"
#include "uci/type/ActivityStateEnum.h"
#include "uci/type/AvailabilityEnum.h"
#include "uci/type/CapabilityCommandStateEnum.h"
#include "uci/type/CapabilityID_Type.h"
#include "uci/type/CapabilityStatusType.h"
#include "uci/type/CommandProcessingStateEnum.h"
#include "uci/type/CommandStateEnum.h"
#include "uci/type/ObjectStateEnum.h"
#include "uci/type/ResultingActivityType.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

constexpr const char* kCapabilityName = "AmtiSurveillanceCapability";
constexpr const char* kTopicCapability = "AMTI_Capability";
constexpr const char* kTopicCapabilityStatus = "AMTI_CapabilityStatus";
constexpr const char* kTopicCommand = "AMTI_Command";
constexpr const char* kTopicCommandStatus = "AMTI_CommandStatus";
constexpr const char* kTopicActivity = "AMTI_Activity";

using Clock = std::chrono::steady_clock;

void setId(uci::type::ID_Type& id, const uci::base::UUID& uuid, const std::string& label) {
    id.setUUID(uuid);
    id.enableDescriptiveLabel() = label;
}

uci::base::UUID newUuid() {
    return uci::base::UUID::generateUUID();
}

void sleepForDiscovery() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void publishCapability(uci::base::AbstractServiceBusConnection* asb,
                       uci::type::AMTI_CapabilityMT::Writer& writer,
                       const uci::base::UUID& capabilityUuid) {
    auto& message = uci::type::AMTI_CapabilityMT::create(asb);
    message.enableObjectState().setValue(uci::type::ObjectStateEnum::NEW);

    auto& capabilities = message.getMessageData().getCapability();
    capabilities.resize(1);
    auto& capability = capabilities[0];
    setId(capability.getCapabilityID(), capabilityUuid, kCapabilityName);
    capability.getCapabilityType().setValue(uci::type::AMTI_CapabilityEnum::VOLUME);
    capability.getCapabilityOptions().setInterruptOtherActivities(false);
    capability.getCapabilityOptions().setCollectionPolicy(true);
    capability.getCapabilityOptions().setConcurrentOperationSupported(false);

    auto& outputs = capability.getMessageOutput();
    outputs.resize(1);
    auto& output = outputs[0];
    output.setValue(uci::type::AMTI_MessageOutputsEnum::AMTI_ACTIVITY);

    writer.write(message);
    uci::type::AMTI_CapabilityMT::destroy(message);
    std::cout << "service: published AMTI_Capability capabilityUUID=" << capabilityUuid.toString() << " [configured]\n";
}

void publishCapabilityStatus(uci::base::AbstractServiceBusConnection* asb,
                             uci::type::AMTI_CapabilityStatusMT::Writer& writer,
                             const uci::base::UUID& capabilityUuid) {
    auto& message = uci::type::AMTI_CapabilityStatusMT::create(asb);
    auto& statuses = message.getMessageData().getCapabilityStatus();
    statuses.resize(1);
    auto& status = statuses[0];

    setId(status.getCapabilityID(), capabilityUuid, kCapabilityName);
    status.getAvailability().setValue(uci::type::AvailabilityEnum::AVAILABLE);
    writer.write(message);
    uci::type::AMTI_CapabilityStatusMT::destroy(message);
    std::cout << "service: published AMTI_CapabilityStatus capabilityUUID=" << capabilityUuid.toString()
              << " availability=AVAILABLE\n";
}

void publishCommandStatus(uci::base::AbstractServiceBusConnection* asb,
                          uci::type::AMTI_CommandStatusMT::Writer& writer,
                          const uci::base::UUID& commandUuid,
                          const uci::base::UUID* activityUuid) {
    auto& message = uci::type::AMTI_CommandStatusMT::create(asb);
    auto& data = message.getMessageData();
    setId(data.getCommandID(), commandUuid, "AMTI command");
    data.getCommandProcessingState().setValue(uci::type::CommandProcessingStateEnum::ACCEPTED);

    if (activityUuid != nullptr) {
        auto& activities = data.getActivity();
        activities.resize(1);
        auto& activity = activities[0];
        setId(activity.getActivityID(), *activityUuid, "AMTI activity");
        activity.setNewActivity(true);
    }

    writer.write(message);
    uci::type::AMTI_CommandStatusMT::destroy(message);
    std::cout << "service: published AMTI_CommandStatus commandUUID=" << commandUuid.toString()
              << " state=ACCEPTED";
    if (activityUuid != nullptr) std::cout << " activityUUID=" << activityUuid->toString();
    std::cout << "\n";
}

void publishActivity(uci::base::AbstractServiceBusConnection* asb,
                     uci::type::AMTI_ActivityMT::Writer& writer,
                     const uci::base::UUID& capabilityUuid,
                     const uci::base::UUID& activityUuid,
                     uci::type::ActivityStateEnum::EnumerationItem state,
                     uci::type::ObjectStateEnum::EnumerationItem objectState) {
    auto& message = uci::type::AMTI_ActivityMT::create(asb);
    message.enableObjectState().setValue(objectState);

    auto& data = message.getMessageData();
    setId(data.getSubsystemID(), asb->getMySubsystemUUID(), "AmtiPayload");

    auto& activities = data.getActivity();
    activities.resize(1);
    auto& activity = activities[0];
    setId(activity.getActivityID(), activityUuid, "AMTI activity");
    activity.setInteractive(false);
    activity.getActivityState().setValue(state);
    activity.setAllProductsAndMessagesProduced(state == uci::type::ActivityStateEnum::COMPLETED);

    auto& capabilityIds = activity.getCapabilityID();
    capabilityIds.resize(1);
    auto& capabilityId = capabilityIds[0];
    setId(capabilityId, capabilityUuid, kCapabilityName);

    writer.write(message);
    std::cout << "service: published AMTI_Activity activityUUID=" << activityUuid.toString()
              << " [dynamic] capabilityUUID=" << capabilityUuid.toString()
              << " state=" << activity.getActivityState().toName() << "\n";
    uci::type::AMTI_ActivityMT::destroy(message);
}

struct ReceivedCommand {
    bool received{false};
    bool start{false};
    uci::base::UUID commandUuid{};
    uci::base::UUID activityUuid{};
};

class CommandListener final : public uci::type::AMTI_CommandMT::Listener {
public:
    void handleMessage(const uci::type::AMTI_CommandMT& message) override {
        const auto& commands = message.getMessageData().getCommand();
        if (commands.empty()) return;

        const auto& command = commands[0];
        if (command.isCapability()) {
            const auto& capability = command.getCapability();
            last.received = true;
            last.start = true;
            last.commandUuid = capability.getCommandID().getUUID();
            last.activityUuid = uci::base::UUID{};
            std::cout << "service: received start AMTI_Command commandUUID=" << last.commandUuid.toString()
                      << " capabilityUUID=" << capability.getCapabilityID().getUUID().toString() << "\n";
        } else if (command.isActivity()) {
            const auto& activity = command.getActivity();
            last.received = true;
            last.start = false;
            last.commandUuid = activity.getCommandID().getUUID();
            last.activityUuid = activity.getActivityID().getUUID();
            std::cout << "service: received stop AMTI_Command commandUUID=" << last.commandUuid.toString()
                      << " activityUUID=" << last.activityUuid.toString() << "\n";
        }
    }

    ReceivedCommand take() {
        auto result = last;
        last = ReceivedCommand{};
        return result;
    }

private:
    ReceivedCommand last;
};

class CommandStatusListener final : public uci::type::AMTI_CommandStatusMT::Listener {
public:
    void handleMessage(const uci::type::AMTI_CommandStatusMT& message) override {
        ++received;
        lastCommandUuid = message.getMessageData().getCommandID().getUUID();
        lastState = message.getMessageData().getCommandProcessingState().toName();
        std::cout << "client: received AMTI_CommandStatus commandUUID=" << lastCommandUuid.toString()
                  << " state=" << lastState << "\n";
    }

    int received{0};
    uci::base::UUID lastCommandUuid;
    std::string lastState;
};

class ActivityListener final : public uci::type::AMTI_ActivityMT::Listener {
public:
    void handleMessage(const uci::type::AMTI_ActivityMT& message) override {
        const auto& activities = message.getMessageData().getActivity();
        if (activities.empty()) return;
        ++received;
        lastActivityUuid = activities[0].getActivityID().getUUID();
        lastState = activities[0].getActivityState().toName();
        std::cout << "client: received AMTI_Activity activityUUID=" << lastActivityUuid.toString()
                  << " state=" << lastState << "\n";
    }

    int received{0};
    uci::base::UUID lastActivityUuid;
    std::string lastState;
};

bool waitForCommand(uci::type::AMTI_CommandMT::Reader& reader,
                    CommandListener& listener,
                    ReceivedCommand& command,
                    unsigned long timeoutMs) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        reader.read(250, 1, listener);
        command = listener.take();
        if (command.received) return true;
    }
    return false;
}

bool waitForClientUpdates(uci::type::AMTI_CommandStatusMT::Reader& statusReader,
                          CommandStatusListener& statusListener,
                          int targetStatusCount,
                          uci::type::AMTI_ActivityMT::Reader& activityReader,
                          ActivityListener& activityListener,
                          const std::string& targetActivityState,
                          unsigned long timeoutMs) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        if (statusListener.received < targetStatusCount) {
            statusReader.read(100, 1, statusListener);
        }
        if (activityListener.lastState != targetActivityState) {
            activityReader.read(100, 1, activityListener);
        }
        if (statusListener.received >= targetStatusCount &&
            activityListener.lastState == targetActivityState) {
            return true;
        }
    }
    return false;
}

void sendStartCommand(uci::base::AbstractServiceBusConnection* asb,
                      uci::type::AMTI_CommandMT::Writer& writer,
                      const uci::base::UUID& capabilityUuid,
                      const uci::base::UUID& commandUuid) {
    auto& message = uci::type::AMTI_CommandMT::create(asb);
    auto& commands = message.getMessageData().getCommand();
    commands.resize(1);
    auto& command = commands[0];

    auto& capabilityCommand = command.chooseCapability();
    setId(capabilityCommand.getCommandID(), commandUuid, "Start AMTI command");
    capabilityCommand.getCommandState().setValue(uci::type::CommandStateEnum::NEW);
    setId(capabilityCommand.getCapabilityID(), capabilityUuid, kCapabilityName);
    capabilityCommand.getRanking().getRank().getPriority().setValue(1);

    writer.write(message);
    uci::type::AMTI_CommandMT::destroy(message);
    std::cout << "client: sent start AMTI_Command commandUUID=" << commandUuid.toString()
              << " [dynamic] capabilityUUID=" << capabilityUuid.toString() << "\n";
}

void sendStopCommand(uci::base::AbstractServiceBusConnection* asb,
                     uci::type::AMTI_CommandMT::Writer& writer,
                     const uci::base::UUID& activityUuid,
                     const uci::base::UUID& commandUuid) {
    auto& message = uci::type::AMTI_CommandMT::create(asb);
    auto& commands = message.getMessageData().getCommand();
    commands.resize(1);
    auto& command = commands[0];

    auto& activityCommand = command.chooseActivity();
    setId(activityCommand.getCommandID(), commandUuid, "Stop AMTI command");
    activityCommand.getCommandState().setValue(uci::type::CommandStateEnum::NEW);
    setId(activityCommand.getActivityID(), activityUuid, "AMTI activity");
    activityCommand.enableChangeActivityState().setValue(uci::type::CapabilityCommandStateEnum::DISABLE);

    writer.write(message);
    uci::type::AMTI_CommandMT::destroy(message);
    std::cout << "client: sent stop AMTI_Command commandUUID=" << commandUuid.toString()
              << " [dynamic] activityUUID=" << activityUuid.toString() << "\n";
}

int runService() {
    auto* asb = uci_getAbstractServiceBusConnection("AmtiSensorService", "DDS");
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName);

    auto& capabilityWriter = uci::type::AMTI_CapabilityMT::createWriter(kTopicCapability, asb);
    auto& capabilityStatusWriter = uci::type::AMTI_CapabilityStatusMT::createWriter(kTopicCapabilityStatus, asb);
    auto& commandStatusWriter = uci::type::AMTI_CommandStatusMT::createWriter(kTopicCommandStatus, asb);
    auto& activityWriter = uci::type::AMTI_ActivityMT::createWriter(kTopicActivity, asb);
    auto& commandReader = uci::type::AMTI_CommandMT::createReader(kTopicCommand, asb);

    try {
        publishCapability(asb, capabilityWriter, capabilityUuid);
        publishCapabilityStatus(asb, capabilityStatusWriter, capabilityUuid);

        CommandListener listener;
        ReceivedCommand command;
        if (!waitForCommand(commandReader, listener, command, 30000)) {
            std::cerr << "service: timed out waiting for start command\n";
            throw std::runtime_error("start command timeout");
        }
        if (!command.start) {
            throw std::runtime_error("first command was not a capability start command");
        }

        const auto activityUuid = newUuid();
        publishCommandStatus(asb, commandStatusWriter, command.commandUuid, &activityUuid);
        publishActivity(asb, activityWriter, capabilityUuid, activityUuid,
                        uci::type::ActivityStateEnum::ACTIVE_UNCONSTRAINED,
                        uci::type::ObjectStateEnum::NEW);

        if (!waitForCommand(commandReader, listener, command, 30000)) {
            std::cerr << "service: timed out waiting for stop command\n";
            throw std::runtime_error("stop command timeout");
        }
        if (command.start || command.activityUuid != activityUuid) {
            throw std::runtime_error("stop command did not reference the active activity");
        }

        publishCommandStatus(asb, commandStatusWriter, command.commandUuid, nullptr);
        publishActivity(asb, activityWriter, capabilityUuid, activityUuid,
                        uci::type::ActivityStateEnum::COMPLETED,
                        uci::type::ObjectStateEnum::UPDATED);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } catch (...) {
        commandReader.close();
        capabilityWriter.close();
        capabilityStatusWriter.close();
        commandStatusWriter.close();
        activityWriter.close();
        uci::type::AMTI_CommandMT::destroyReader(commandReader);
        uci::type::AMTI_CapabilityMT::destroyWriter(capabilityWriter);
        uci::type::AMTI_CapabilityStatusMT::destroyWriter(capabilityStatusWriter);
        uci::type::AMTI_CommandStatusMT::destroyWriter(commandStatusWriter);
        uci::type::AMTI_ActivityMT::destroyWriter(activityWriter);
        asb->shutdown();
        uci_destroyAbstractServiceBusConnection(asb);
        throw;
    }

    commandReader.close();
    capabilityWriter.close();
    capabilityStatusWriter.close();
    commandStatusWriter.close();
    activityWriter.close();
    uci::type::AMTI_CommandMT::destroyReader(commandReader);
    uci::type::AMTI_CapabilityMT::destroyWriter(capabilityWriter);
    uci::type::AMTI_CapabilityStatusMT::destroyWriter(capabilityStatusWriter);
    uci::type::AMTI_CommandStatusMT::destroyWriter(commandStatusWriter);
    uci::type::AMTI_ActivityMT::destroyWriter(activityWriter);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    return 0;
}

int runClient() {
    auto* asb = uci_getAbstractServiceBusConnection("AmtiOperatorService", "DDS");
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName);

    auto& commandWriter = uci::type::AMTI_CommandMT::createWriter(kTopicCommand, asb);
    auto& commandStatusReader = uci::type::AMTI_CommandStatusMT::createReader(kTopicCommandStatus, asb);
    auto& activityReader = uci::type::AMTI_ActivityMT::createReader(kTopicActivity, asb);

    try {
        CommandStatusListener statusListener;
        ActivityListener activityListener;
        sleepForDiscovery();

        sendStartCommand(asb, commandWriter, capabilityUuid, newUuid());
        if (!waitForClientUpdates(commandStatusReader, statusListener, 1,
                                  activityReader, activityListener,
                                  "ACTIVE_UNCONSTRAINED", 10000)) {
            throw std::runtime_error("client timed out waiting for active activity");
        }

        const auto activityUuid = activityListener.lastActivityUuid;
        sendStopCommand(asb, commandWriter, activityUuid, newUuid());
        if (!waitForClientUpdates(commandStatusReader, statusListener, 2,
                                  activityReader, activityListener,
                                  "COMPLETED", 10000)) {
            throw std::runtime_error("client timed out waiting for completed activity");
        }
    } catch (...) {
        commandWriter.close();
        commandStatusReader.close();
        activityReader.close();
        uci::type::AMTI_CommandMT::destroyWriter(commandWriter);
        uci::type::AMTI_CommandStatusMT::destroyReader(commandStatusReader);
        uci::type::AMTI_ActivityMT::destroyReader(activityReader);
        asb->shutdown();
        uci_destroyAbstractServiceBusConnection(asb);
        throw;
    }

    commandWriter.close();
    commandStatusReader.close();
    activityReader.close();
    uci::type::AMTI_CommandMT::destroyWriter(commandWriter);
    uci::type::AMTI_CommandStatusMT::destroyReader(commandStatusReader);
    uci::type::AMTI_ActivityMT::destroyReader(activityReader);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    return 0;
}

void usage(const char* program) {
    std::cerr << "usage: " << program << " service|client [--demo]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    try {
        const std::string mode = argv[1];
        if (mode == "service") return runService();
        if (mode == "client") return runClient();
        usage(argv[0]);
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
