#include "uci/type/ActivityStateEnum.h"
#include "uci/type/AvailabilityEnum.h"
#include "uci/type/CapabilityCommandStateEnum.h"
#include "uci/type/CapabilityID_Type.h"
#include "uci/type/CapabilityStatusType.h"
#include "uci/type/CommandProcessingStateEnum.h"
#include "uci/type/CommandStateEnum.h"
#include "uci/type/ObjectStateEnum.h"
#include "uci/type/ResultingActivityType.h"
#include "uci/type/SMTI_ActivityMT.h"
#include "uci/type/SMTI_ActivityType.h"
#include "uci/type/SMTI_CapabilityMT.h"
#include "uci/type/SMTI_CapabilityStatusMT.h"
#include "uci/type/SMTI_CapabilityType.h"
#include "uci/type/SMTI_CommandMT.h"
#include "uci/type/SMTI_CommandStatusMT.h"
#include "uci/type/SMTI_CommandType.h"
#include "uci/type/SMTI_MessageOutputsEnum.h"
#include "uci/utils/All.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

constexpr const char* kCapabilityName = "SmtiSurveillanceCapability";
constexpr const char* kTopicCapability = "SMTI_Capability";
constexpr const char* kTopicCapabilityStatus = "SMTI_CapabilityStatus";
constexpr const char* kTopicCommand = "SMTI_Command";
constexpr const char* kTopicCommandStatus = "SMTI_CommandStatus";
constexpr const char* kTopicActivity = "SMTI_Activity";

using Clock = std::chrono::steady_clock;

void sleepForDiscovery() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void publishCapability(uci::base::AbstractServiceBusConnection* asb,
                       uci::utils::WriterPtr<uci::type::SMTI_CapabilityMT>& writer,
                       const uci::base::UUID& capabilityUuid) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_CapabilityMT>(asb);
    uci::utils::set(message->enableObjectState(), uci::type::ObjectStateEnum::NEW);

    auto& capabilities = message->getMessageData().getCapability();
    capabilities.resize(1);
    auto& capability = capabilities[0];
    uci::utils::setId(capability.getCapabilityID(), capabilityUuid, kCapabilityName);
    uci::utils::set(capability.getCapabilityType(), uci::type::SMTI_CapabilityEnum::AREA);
    uci::utils::set(capability.getSubCapabilityType(), uci::type::SMTI_SubCapabilityEnum::GMTI);
    capability.getCapabilityOptions().setInterruptOtherActivities(false);
    capability.getCapabilityOptions().setCollectionPolicy(true);
    capability.getCapabilityOptions().setTrackingSupported(true);
    capability.getCapabilityOptions().setConcurrentOperationSupported(false);

    auto& outputs = capability.getMessageOutput();
    outputs.resize(1);
    auto& output = outputs[0];
    uci::utils::set(output, uci::type::SMTI_MessageOutputsEnum::SMTI_ACTIVITY);

    writer->write(*message);
    std::cout << "service: published SMTI_Capability capabilityUUID=" << capabilityUuid.toString() << " [configured]\n";
}

void publishCapabilityStatus(uci::base::AbstractServiceBusConnection* asb,
                             uci::utils::WriterPtr<uci::type::SMTI_CapabilityStatusMT>& writer,
                             const uci::base::UUID& capabilityUuid) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_CapabilityStatusMT>(asb);
    auto& statuses = message->getMessageData().getCapabilityStatus();
    statuses.resize(1);
    auto& status = statuses[0];
    uci::utils::setId(status.getCapabilityID(), capabilityUuid, kCapabilityName);
    uci::utils::set(status.getAvailability(), uci::type::AvailabilityEnum::AVAILABLE);

    writer->write(*message);
    std::cout << "service: published SMTI_CapabilityStatus capabilityUUID=" << capabilityUuid.toString()
              << " availability=AVAILABLE\n";
}

void publishCommandStatus(uci::base::AbstractServiceBusConnection* asb,
                          uci::utils::WriterPtr<uci::type::SMTI_CommandStatusMT>& writer,
                          const uci::base::UUID& commandUuid,
                          const uci::base::UUID* activityUuid) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_CommandStatusMT>(asb);
    auto& data = message->getMessageData();
    uci::utils::setId(data.getCommandID(), commandUuid, "SMTI command");
    uci::utils::set(data.getCommandProcessingState(), uci::type::CommandProcessingStateEnum::ACCEPTED);

    if (activityUuid != nullptr) {
        auto& activities = data.getActivity();
        activities.resize(1);
        auto& activity = activities[0];
        uci::utils::setId(activity.getActivityID(), *activityUuid, "SMTI activity");
        activity.setNewActivity(true);
    }

    writer->write(*message);
    std::cout << "service: published SMTI_CommandStatus commandUUID=" << commandUuid.toString()
              << " state=ACCEPTED";
    if (activityUuid != nullptr) std::cout << " activityUUID=" << activityUuid->toString();
    std::cout << "\n";
}

void publishActivity(uci::base::AbstractServiceBusConnection* asb,
                     uci::utils::WriterPtr<uci::type::SMTI_ActivityMT>& writer,
                     const uci::base::UUID& capabilityUuid,
                     const uci::base::UUID& activityUuid,
                     uci::type::ActivityStateEnum::EnumerationItem state,
                     uci::type::ObjectStateEnum::EnumerationItem objectState) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_ActivityMT>(asb);
    uci::utils::set(message->enableObjectState(), objectState);

    auto& data = message->getMessageData();
    uci::utils::setId(data.getSubsystemID(), asb->getMySubsystemUUID(), "SmtiPayload");

    auto& activities = data.getActivity();
    activities.resize(1);
    auto& activity = activities[0];
    uci::utils::setId(activity.getActivityID(), activityUuid, "SMTI activity");
    activity.setInteractive(false);
    uci::utils::set(activity.getActivityState(), state);
    activity.setAllProductsAndMessagesProduced(state == uci::type::ActivityStateEnum::COMPLETED);

    auto& capabilityIds = activity.getCapabilityID();
    capabilityIds.resize(1);
    auto& capabilityId = capabilityIds[0];
    uci::utils::setId(capabilityId, capabilityUuid, kCapabilityName);

    writer->write(*message);
    std::cout << "service: published SMTI_Activity activityUUID=" << activityUuid.toString()
              << " [dynamic] capabilityUUID=" << capabilityUuid.toString()
              << " state=" << activity.getActivityState().toName() << "\n";
}

struct ReceivedCommand {
    bool received{false};
    bool start{false};
    uci::base::UUID commandUuid;
    uci::base::UUID activityUuid;
};

bool waitForCommand(uci::utils::ReaderPtr<uci::type::SMTI_CommandMT>& reader,
                    uci::utils::FunctionListener<uci::type::SMTI_CommandMT>& listener,
                    ReceivedCommand& command,
                    unsigned long timeoutMs) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        reader->read(250, 1, listener);
        if (command.received) return true;
    }
    return false;
}

bool waitForClientUpdates(uci::utils::ReaderPtr<uci::type::SMTI_CommandStatusMT>& statusReader,
                          uci::utils::FunctionListener<uci::type::SMTI_CommandStatusMT>& statusListener,
                          int& statusCount,
                          int targetStatusCount,
                          uci::utils::ReaderPtr<uci::type::SMTI_ActivityMT>& activityReader,
                          uci::utils::FunctionListener<uci::type::SMTI_ActivityMT>& activityListener,
                          std::string& lastActivityState,
                          const std::string& targetActivityState,
                          unsigned long timeoutMs) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        if (statusCount < targetStatusCount) {
            statusReader->read(100, 1, statusListener);
        }
        if (lastActivityState != targetActivityState) {
            activityReader->read(100, 1, activityListener);
        }
        if (statusCount >= targetStatusCount && lastActivityState == targetActivityState) {
            return true;
        }
    }
    return false;
}

uci::base::UUID sendStartCommand(uci::base::AbstractServiceBusConnection* asb,
                                 uci::utils::WriterPtr<uci::type::SMTI_CommandMT>& writer,
                                 const uci::base::UUID& capabilityUuid) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_CommandMT>(asb);
    auto& commands = message->getMessageData().getCommand();
    commands.resize(1);
    auto& command = commands[0];

    auto& capabilityCommand = command.chooseCapability();
    const auto commandUuid = uci::utils::assignNewId(capabilityCommand.getCommandID(),
                                                     "Start SMTI command");
    uci::utils::set(capabilityCommand.getCommandState(), uci::type::CommandStateEnum::NEW);
    uci::utils::setId(capabilityCommand.getCapabilityID(), capabilityUuid, kCapabilityName);
    capabilityCommand.getRanking().getRank().getPriority().setValue(1);

    writer->write(*message);
    std::cout << "client: sent start SMTI_Command commandUUID=" << commandUuid.toString()
              << " [dynamic] capabilityUUID=" << capabilityUuid.toString() << "\n";
    return commandUuid;
}

uci::base::UUID sendStopCommand(uci::base::AbstractServiceBusConnection* asb,
                                uci::utils::WriterPtr<uci::type::SMTI_CommandMT>& writer,
                                const uci::base::UUID& activityUuid) {
    auto message = uci::utils::makeMessage<uci::type::SMTI_CommandMT>(asb);
    auto& commands = message->getMessageData().getCommand();
    commands.resize(1);
    auto& command = commands[0];

    auto& activityCommand = command.chooseActivity();
    const auto commandUuid = uci::utils::assignNewId(activityCommand.getCommandID(),
                                                     "Stop SMTI command");
    uci::utils::set(activityCommand.getCommandState(), uci::type::CommandStateEnum::NEW);
    uci::utils::setId(activityCommand.getActivityID(), activityUuid, "SMTI activity");
    uci::utils::set(activityCommand.enableChangeActivityState(),
                    uci::type::CapabilityCommandStateEnum::DISABLE);

    writer->write(*message);
    std::cout << "client: sent stop SMTI_Command commandUUID=" << commandUuid.toString()
              << " [dynamic] activityUUID=" << activityUuid.toString() << "\n";
    return commandUuid;
}

int runService() {
    auto asb = uci::utils::makeConnection("SmtiSensorService");
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName);

    auto capabilityWriter = uci::utils::makeWriter<uci::type::SMTI_CapabilityMT>(kTopicCapability, asb.get());
    auto capabilityStatusWriter = uci::utils::makeWriter<uci::type::SMTI_CapabilityStatusMT>(kTopicCapabilityStatus, asb.get());
    auto commandStatusWriter = uci::utils::makeWriter<uci::type::SMTI_CommandStatusMT>(kTopicCommandStatus, asb.get());
    auto activityWriter = uci::utils::makeWriter<uci::type::SMTI_ActivityMT>(kTopicActivity, asb.get());
    auto commandReader = uci::utils::makeReader<uci::type::SMTI_CommandMT>(kTopicCommand, asb.get());

    ReceivedCommand command;
    uci::utils::FunctionListener<uci::type::SMTI_CommandMT> commandListener(
        [&](const uci::type::SMTI_CommandMT& message) {
            const auto& commands = message.getMessageData().getCommand();
            if (commands.empty()) return;

            const auto& commandChoice = commands[0];
            command = {};
            command.received = true;
            if (commandChoice.isCapability()) {
                const auto& capability = commandChoice.getCapability();
                command.start = true;
                command.commandUuid = uci::utils::uuidOf(capability.getCommandID());
                std::cout << "service: received start SMTI_Command commandUUID=" << command.commandUuid.toString()
                          << " capabilityUUID=" << uci::utils::uuidOf(capability.getCapabilityID()).toString() << "\n";
            } else if (commandChoice.isActivity()) {
                const auto& activity = commandChoice.getActivity();
                command.start = false;
                command.commandUuid = uci::utils::uuidOf(activity.getCommandID());
                command.activityUuid = uci::utils::uuidOf(activity.getActivityID());
                std::cout << "service: received stop SMTI_Command commandUUID=" << command.commandUuid.toString()
                          << " activityUUID=" << command.activityUuid.toString() << "\n";
            }
        });

    publishCapability(asb.get(), capabilityWriter, capabilityUuid);
    publishCapabilityStatus(asb.get(), capabilityStatusWriter, capabilityUuid);

    if (!waitForCommand(commandReader, commandListener, command, 30000) || !command.start) {
        throw std::runtime_error("service timed out waiting for start command");
    }

    const auto activityUuid = uci::utils::newUuid();
    publishCommandStatus(asb.get(), commandStatusWriter, command.commandUuid, &activityUuid);
    publishActivity(asb.get(), activityWriter, capabilityUuid, activityUuid,
                    uci::type::ActivityStateEnum::ACTIVE_UNCONSTRAINED,
                    uci::type::ObjectStateEnum::NEW);

    command = {};
    if (!waitForCommand(commandReader, commandListener, command, 30000) ||
        command.start || command.activityUuid != activityUuid) {
        throw std::runtime_error("service timed out waiting for stop command");
    }

    publishCommandStatus(asb.get(), commandStatusWriter, command.commandUuid, nullptr);
    publishActivity(asb.get(), activityWriter, capabilityUuid, activityUuid,
                    uci::type::ActivityStateEnum::COMPLETED,
                    uci::type::ObjectStateEnum::UPDATED);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}

int runClient() {
    auto asb = uci::utils::makeConnection("SmtiOperatorService");
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName);

    auto commandWriter = uci::utils::makeWriter<uci::type::SMTI_CommandMT>(kTopicCommand, asb.get());
    auto commandStatusReader = uci::utils::makeReader<uci::type::SMTI_CommandStatusMT>(kTopicCommandStatus, asb.get());
    auto activityReader = uci::utils::makeReader<uci::type::SMTI_ActivityMT>(kTopicActivity, asb.get());

    int statusCount = 0;
    uci::base::UUID lastActivityUuid;
    std::string lastActivityState;

    uci::utils::FunctionListener<uci::type::SMTI_CommandStatusMT> statusListener(
        [&](const uci::type::SMTI_CommandStatusMT& message) {
            ++statusCount;
            std::cout << "client: received SMTI_CommandStatus commandUUID="
                      << uci::utils::uuidOf(message.getMessageData().getCommandID()).toString()
                      << " state=" << message.getMessageData().getCommandProcessingState().toName()
                      << "\n";
        });

    uci::utils::FunctionListener<uci::type::SMTI_ActivityMT> activityListener(
        [&](const uci::type::SMTI_ActivityMT& message) {
            const auto& activities = message.getMessageData().getActivity();
            if (activities.empty()) return;
            lastActivityUuid = uci::utils::uuidOf(activities[0].getActivityID());
            lastActivityState = activities[0].getActivityState().toName();
            std::cout << "client: received SMTI_Activity activityUUID=" << lastActivityUuid.toString()
                      << " state=" << lastActivityState << "\n";
        });

    sleepForDiscovery();
    sendStartCommand(asb.get(), commandWriter, capabilityUuid);
    if (!waitForClientUpdates(commandStatusReader, statusListener, statusCount, 1,
                              activityReader, activityListener, lastActivityState,
                              "ACTIVE_UNCONSTRAINED", 10000)) {
        throw std::runtime_error("client timed out waiting for active activity");
    }

    sendStopCommand(asb.get(), commandWriter, lastActivityUuid);
    if (!waitForClientUpdates(commandStatusReader, statusListener, statusCount, 2,
                              activityReader, activityListener, lastActivityState,
                              "COMPLETED", 10000)) {
        throw std::runtime_error("client timed out waiting for completed activity");
    }
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
