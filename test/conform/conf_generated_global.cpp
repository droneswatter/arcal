// Compile-only conformance: representative generated global element API shape.
#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/Listener.h"
#include "uci/base/Reader.h"
#include "uci/base/Writer.h"
#include "uci/type/ActionCommandMT.h"

#include <string>
#include <type_traits>

using Msg = uci::type::ActionCommandMT;
using Listener = Msg::Listener;
using Reader = Msg::Reader;
using Writer = Msg::Writer;

static_assert(std::is_base_of_v<uci::base::Accessor, Msg>);
static_assert(std::is_base_of_v<uci::base::Listener, Listener>);
static_assert(std::is_base_of_v<uci::base::Reader, Reader>);
static_assert(std::is_base_of_v<uci::base::Writer, Writer>);

using HandleFn = void (Listener::*)(const Msg&);
using AddListenerFn = void (Reader::*)(Listener&);
using RemoveListenerFn = void (Reader::*)(Listener&);
using ReadFn = unsigned long (Reader::*)(unsigned long, unsigned long, Listener&);
using ReadNoWaitFn = unsigned long (Reader::*)(unsigned long, Listener&);
using ReaderCloseFn = void (Reader::*)();
using WriteFn = void (Writer::*)(Msg&);
using WriterCloseFn = void (Writer::*)();

static_assert(std::is_same_v<HandleFn, decltype(&Listener::handleMessage)>);
static_assert(std::is_same_v<AddListenerFn, decltype(&Reader::addListener)>);
static_assert(std::is_same_v<RemoveListenerFn, decltype(&Reader::removeListener)>);
static_assert(std::is_same_v<ReadFn, decltype(&Reader::read)>);
static_assert(std::is_same_v<ReadNoWaitFn, decltype(&Reader::readNoWait)>);
static_assert(std::is_same_v<ReaderCloseFn, decltype(&Reader::close)>);
static_assert(std::is_same_v<WriteFn, decltype(&Writer::write)>);
static_assert(std::is_same_v<WriterCloseFn, decltype(&Writer::close)>);

using CreateReaderFn = Reader& (*)(const std::string&, uci::base::AbstractServiceBusConnection*);
using DestroyReaderFn = void (*)(Reader&);
using CreateWriterFn = Writer& (*)(const std::string&, uci::base::AbstractServiceBusConnection*);
using DestroyWriterFn = void (*)(Writer&);

static_assert(std::is_same_v<CreateReaderFn, decltype(&Msg::createReader)>);
static_assert(std::is_same_v<DestroyReaderFn, decltype(&Msg::destroyReader)>);
static_assert(std::is_same_v<CreateWriterFn, decltype(&Msg::createWriter)>);
static_assert(std::is_same_v<DestroyWriterFn, decltype(&Msg::destroyWriter)>);

static_assert(noexcept(std::declval<Msg&>().getAccessorType()));
static_assert(std::is_same_v<const std::string&, decltype(std::declval<const Msg&>().typeName())>);
static_assert(std::is_same_v<std::string, decltype(Msg::getUCITypeVersion())>);
