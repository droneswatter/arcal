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
#include <memory>
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

struct AsbDeleter {
    void operator()(uci::base::AbstractServiceBusConnection* asb) const {
        if (asb != nullptr) uci_destroyAbstractServiceBusConnection(asb);
    }
};

using AsbPtr = std::unique_ptr<uci::base::AbstractServiceBusConnection, AsbDeleter>;

// RAII wrapper for the CxxCAL accessor lifecycle. The explicit T::create(asb)
// and T::destroy(accessor) calls remain visible in this sample on purpose.
template <typename T, void (*Destroy)(T&)>
class AccessorPtr {
public:
    explicit AccessorPtr(T& value) : value_(&value) {}
    ~AccessorPtr() {
        if (value_ != nullptr) Destroy(*value_);
    }

    AccessorPtr(const AccessorPtr&) = delete;
    AccessorPtr& operator=(const AccessorPtr&) = delete;

    T& get() const { return *value_; }
    T* operator->() const { return value_; }

private:
    T* value_;
};

void setId(uci::type::ID_Type& id, const std::string& uuid, const std::string& label) {
    id.getUUID().setValue(uuid);
    id.enableDescriptiveLabel().setValue(label);
}

std::string newUuid() {
    return uci::base::UUID::generateUUID().toString();
}

void sleepForDiscovery() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void publishCapability(uci::base::AbstractServiceBusConnection* asb,
                       uci::type::AMTI_CapabilityMT::Writer& writer,
                       const std::string& capabilityUuid) {
    AccessorPtr<uci::type::AMTI_CapabilityMT, uci::type::AMTI_CapabilityMT::destroy>
        message(uci::type::AMTI_CapabilityMT::create(asb));
    message->enableObjectState().setValue(uci::type::ObjectStateEnum::NEW);

    AccessorPtr<uci::type::AMTI_CapabilityType, uci::type::AMTI_CapabilityType::destroy>
        capability(uci::type::AMTI_CapabilityType::create(asb));
    setId(capability->getCapabilityID(), capabilityUuid, kCapabilityName);
    capability->getCapabilityType().setValue(uci::type::AMTI_CapabilityEnum::VOLUME);
    capability->getCapabilityOptions().setInterruptOtherActivities(false);
    capability->getCapabilityOptions().setCollectionPolicy(true);
    capability->getCapabilityOptions().setConcurrentOperationSupported(false);

    AccessorPtr<uci::type::AMTI_MessageOutputsEnum, uci::type::AMTI_MessageOutputsEnum::destroy>
        output(uci::type::AMTI_MessageOutputsEnum::create(asb));
    output->setValue(uci::type::AMTI_MessageOutputsEnum::AMTI_ACTIVITY);
    capability->getMessageOutput().push_back(output.get());

    message->getMessageData().getCapability().push_back(capability.get());
    writer.write(message.get());
    std::cout << "service: published AMTI_Capability capabilityUUID=" << capabilityUuid << " [configured]\n";
}

void publishCapabilityStatus(uci::base::AbstractServiceBusConnection* asb,
                             uci::type::AMTI_CapabilityStatusMT::Writer& writer,
                             const std::string& capabilityUuid) {
    AccessorPtr<uci::type::AMTI_CapabilityStatusMT, uci::type::AMTI_CapabilityStatusMT::destroy>
        message(uci::type::AMTI_CapabilityStatusMT::create(asb));
    AccessorPtr<uci::type::CapabilityStatusType, uci::type::CapabilityStatusType::destroy>
        status(uci::type::CapabilityStatusType::create(asb));

    setId(status->getCapabilityID(), capabilityUuid, kCapabilityName);
    status->getAvailability().setValue(uci::type::AvailabilityEnum::AVAILABLE);
    message->getMessageData().getCapabilityStatus().push_back(status.get());
    writer.write(message.get());
    std::cout << "service: published AMTI_CapabilityStatus capabilityUUID=" << capabilityUuid
              << " availability=AVAILABLE\n";
}

void publishCommandStatus(uci::base::AbstractServiceBusConnection* asb,
                          uci::type::AMTI_CommandStatusMT::Writer& writer,
                          const std::string& commandUuid,
                          const std::string& activityUuid) {
    AccessorPtr<uci::type::AMTI_CommandStatusMT, uci::type::AMTI_CommandStatusMT::destroy>
        message(uci::type::AMTI_CommandStatusMT::create(asb));
    auto& data = message->getMessageData();
    setId(data.getCommandID(), commandUuid, "AMTI command");
    data.getCommandProcessingState().setValue(uci::type::CommandProcessingStateEnum::ACCEPTED);

    if (!activityUuid.empty()) {
        AccessorPtr<uci::type::ResultingActivityType, uci::type::ResultingActivityType::destroy>
            activity(uci::type::ResultingActivityType::create(asb));
        setId(activity->getActivityID(), activityUuid, "AMTI activity");
        activity->setNewActivity(true);
        data.getActivity().push_back(activity.get());
    }

    writer.write(message.get());
    std::cout << "service: published AMTI_CommandStatus commandUUID=" << commandUuid
              << " state=ACCEPTED";
    if (!activityUuid.empty()) std::cout << " activityUUID=" << activityUuid;
    std::cout << "\n";
}

void publishActivity(uci::base::AbstractServiceBusConnection* asb,
                     uci::type::AMTI_ActivityMT::Writer& writer,
                     const std::string& capabilityUuid,
                     const std::string& activityUuid,
                     uci::type::ActivityStateEnum::EnumerationItem state,
                     uci::type::ObjectStateEnum::EnumerationItem objectState) {
    AccessorPtr<uci::type::AMTI_ActivityMT, uci::type::AMTI_ActivityMT::destroy>
        message(uci::type::AMTI_ActivityMT::create(asb));
    message->enableObjectState().setValue(objectState);

    auto& data = message->getMessageData();
    setId(data.getSubsystemID(), asb->getMySubsystemUUID().toString(), "AmtiPayload");

    AccessorPtr<uci::type::AMTI_ActivityType, uci::type::AMTI_ActivityType::destroy>
        activity(uci::type::AMTI_ActivityType::create(asb));
    setId(activity->getActivityID(), activityUuid, "AMTI activity");
    activity->setInteractive(false);
    activity->getActivityState().setValue(state);
    activity->setAllProductsAndMessagesProduced(state == uci::type::ActivityStateEnum::COMPLETED);

    AccessorPtr<uci::type::CapabilityID_Type, uci::type::CapabilityID_Type::destroy>
        capabilityId(uci::type::CapabilityID_Type::create(asb));
    setId(capabilityId.get(), capabilityUuid, kCapabilityName);
    activity->getCapabilityID().push_back(capabilityId.get());

    data.getActivity().push_back(activity.get());
    writer.write(message.get());
    std::cout << "service: published AMTI_Activity activityUUID=" << activityUuid
              << " [dynamic] capabilityUUID=" << capabilityUuid
              << " state=" << activity->getActivityState().toName() << "\n";
}

struct ReceivedCommand {
    bool received{false};
    bool start{false};
    std::string commandUuid;
    std::string activityUuid;
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
            last.commandUuid = capability.getCommandID().getUUID().getValue();
            last.activityUuid.clear();
            std::cout << "service: received start AMTI_Command commandUUID=" << last.commandUuid
                      << " capabilityUUID=" << capability.getCapabilityID().getUUID().getValue() << "\n";
        } else if (command.isActivity()) {
            const auto& activity = command.getActivity();
            last.received = true;
            last.start = false;
            last.commandUuid = activity.getCommandID().getUUID().getValue();
            last.activityUuid = activity.getActivityID().getUUID().getValue();
            std::cout << "service: received stop AMTI_Command commandUUID=" << last.commandUuid
                      << " activityUUID=" << last.activityUuid << "\n";
        }
    }

    ReceivedCommand take() {
        auto result = last;
        last = {};
        return result;
    }

private:
    ReceivedCommand last;
};

class CommandStatusListener final : public uci::type::AMTI_CommandStatusMT::Listener {
public:
    void handleMessage(const uci::type::AMTI_CommandStatusMT& message) override {
        ++received;
        lastCommandUuid = message.getMessageData().getCommandID().getUUID().getValue();
        lastState = message.getMessageData().getCommandProcessingState().toName();
        std::cout << "client: received AMTI_CommandStatus commandUUID=" << lastCommandUuid
                  << " state=" << lastState << "\n";
    }

    int received{0};
    std::string lastCommandUuid;
    std::string lastState;
};

class ActivityListener final : public uci::type::AMTI_ActivityMT::Listener {
public:
    void handleMessage(const uci::type::AMTI_ActivityMT& message) override {
        const auto& activities = message.getMessageData().getActivity();
        if (activities.empty()) return;
        ++received;
        lastActivityUuid = activities[0].getActivityID().getUUID().getValue();
        lastState = activities[0].getActivityState().toName();
        std::cout << "client: received AMTI_Activity activityUUID=" << lastActivityUuid
                  << " state=" << lastState << "\n";
    }

    int received{0};
    std::string lastActivityUuid;
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
                      const std::string& capabilityUuid,
                      const std::string& commandUuid) {
    AccessorPtr<uci::type::AMTI_CommandMT, uci::type::AMTI_CommandMT::destroy>
        message(uci::type::AMTI_CommandMT::create(asb));
    AccessorPtr<uci::type::AMTI_CommandType, uci::type::AMTI_CommandType::destroy>
        command(uci::type::AMTI_CommandType::create(asb));

    auto& capabilityCommand = command->chooseCapability();
    setId(capabilityCommand.getCommandID(), commandUuid, "Start AMTI command");
    capabilityCommand.getCommandState().setValue(uci::type::CommandStateEnum::NEW);
    setId(capabilityCommand.getCapabilityID(), capabilityUuid, kCapabilityName);
    capabilityCommand.getRanking().getRank().getPriority().setValue(1);

    message->getMessageData().getCommand().push_back(command.get());
    writer.write(message.get());
    std::cout << "client: sent start AMTI_Command commandUUID=" << commandUuid
              << " [dynamic] capabilityUUID=" << capabilityUuid << "\n";
}

void sendStopCommand(uci::base::AbstractServiceBusConnection* asb,
                     uci::type::AMTI_CommandMT::Writer& writer,
                     const std::string& activityUuid,
                     const std::string& commandUuid) {
    AccessorPtr<uci::type::AMTI_CommandMT, uci::type::AMTI_CommandMT::destroy>
        message(uci::type::AMTI_CommandMT::create(asb));
    AccessorPtr<uci::type::AMTI_CommandType, uci::type::AMTI_CommandType::destroy>
        command(uci::type::AMTI_CommandType::create(asb));

    auto& activityCommand = command->chooseActivity();
    setId(activityCommand.getCommandID(), commandUuid, "Stop AMTI command");
    activityCommand.getCommandState().setValue(uci::type::CommandStateEnum::NEW);
    setId(activityCommand.getActivityID(), activityUuid, "AMTI activity");
    activityCommand.enableChangeActivityState().setValue(uci::type::CapabilityCommandStateEnum::DISABLE);

    message->getMessageData().getCommand().push_back(command.get());
    writer.write(message.get());
    std::cout << "client: sent stop AMTI_Command commandUUID=" << commandUuid
              << " [dynamic] activityUUID=" << activityUuid << "\n";
}

int runService() {
    AsbPtr asb(uci_getAbstractServiceBusConnection("AmtiSensorService", "DDS"));
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName).toString();

    auto& capabilityWriter = uci::type::AMTI_CapabilityMT::createWriter(kTopicCapability, asb.get());
    auto& capabilityStatusWriter = uci::type::AMTI_CapabilityStatusMT::createWriter(kTopicCapabilityStatus, asb.get());
    auto& commandStatusWriter = uci::type::AMTI_CommandStatusMT::createWriter(kTopicCommandStatus, asb.get());
    auto& activityWriter = uci::type::AMTI_ActivityMT::createWriter(kTopicActivity, asb.get());
    auto& commandReader = uci::type::AMTI_CommandMT::createReader(kTopicCommand, asb.get());

    try {
        publishCapability(asb.get(), capabilityWriter, capabilityUuid);
        publishCapabilityStatus(asb.get(), capabilityStatusWriter, capabilityUuid);

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
        publishCommandStatus(asb.get(), commandStatusWriter, command.commandUuid, activityUuid);
        publishActivity(asb.get(), activityWriter, capabilityUuid, activityUuid,
                        uci::type::ActivityStateEnum::ACTIVE_UNCONSTRAINED,
                        uci::type::ObjectStateEnum::NEW);

        if (!waitForCommand(commandReader, listener, command, 30000)) {
            std::cerr << "service: timed out waiting for stop command\n";
            throw std::runtime_error("stop command timeout");
        }
        if (command.start || command.activityUuid != activityUuid) {
            throw std::runtime_error("stop command did not reference the active activity");
        }

        publishCommandStatus(asb.get(), commandStatusWriter, command.commandUuid, "");
        publishActivity(asb.get(), activityWriter, capabilityUuid, activityUuid,
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
    return 0;
}

int runClient() {
    AsbPtr asb(uci_getAbstractServiceBusConnection("AmtiOperatorService", "DDS"));
    const auto capabilityUuid = asb->getMyCapabilityUUID(kCapabilityName).toString();

    auto& commandWriter = uci::type::AMTI_CommandMT::createWriter(kTopicCommand, asb.get());
    auto& commandStatusReader = uci::type::AMTI_CommandStatusMT::createReader(kTopicCommandStatus, asb.get());
    auto& activityReader = uci::type::AMTI_ActivityMT::createReader(kTopicActivity, asb.get());

    try {
        CommandStatusListener statusListener;
        ActivityListener activityListener;
        sleepForDiscovery();

        sendStartCommand(asb.get(), commandWriter, capabilityUuid, newUuid());
        if (!waitForClientUpdates(commandStatusReader, statusListener, 1,
                                  activityReader, activityListener,
                                  "ACTIVE_UNCONSTRAINED", 10000)) {
            throw std::runtime_error("client timed out waiting for active activity");
        }

        const auto activityUuid = activityListener.lastActivityUuid;
        sendStopCommand(asb.get(), commandWriter, activityUuid, newUuid());
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
        throw;
    }

    commandWriter.close();
    commandStatusReader.close();
    activityReader.close();
    uci::type::AMTI_CommandMT::destroyWriter(commandWriter);
    uci::type::AMTI_CommandStatusMT::destroyReader(commandStatusReader);
    uci::type::AMTI_ActivityMT::destroyReader(activityReader);
    asb->shutdown();
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
