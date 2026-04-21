// Compile-only conformance: Externalizer and ExternalizerLoader API shape.
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

using Accessor = uci::base::Accessor;
using Externalizer = uci::base::Externalizer;
using Loader = uci::base::ExternalizerLoader;

static_assert(std::is_abstract_v<Externalizer>);
static_assert(std::is_abstract_v<Loader>);

using ReadStreamFn = void (Externalizer::*)(std::istream&, Accessor&);
using ReadStringFn = void (Externalizer::*)(const std::string&, Accessor&);
using ReadVectorFn = void (Externalizer::*)(const std::vector<uint8_t>&, Accessor&);
using WriteStreamFn = void (Externalizer::*)(const Accessor&, std::ostream&);
using WriteStringFn = void (Externalizer::*)(const Accessor&, std::string&);
using WriteVectorFn = void (Externalizer::*)(const Accessor&, std::vector<uint8_t>&);

static_assert(std::is_same_v<ReadStreamFn, decltype(static_cast<ReadStreamFn>(&Externalizer::read))>);
static_assert(std::is_same_v<ReadStringFn, decltype(static_cast<ReadStringFn>(&Externalizer::read))>);
static_assert(std::is_same_v<ReadVectorFn, decltype(static_cast<ReadVectorFn>(&Externalizer::read))>);
static_assert(std::is_same_v<WriteStreamFn, decltype(static_cast<WriteStreamFn>(&Externalizer::write))>);
static_assert(std::is_same_v<WriteStringFn, decltype(static_cast<WriteStringFn>(&Externalizer::write))>);
static_assert(std::is_same_v<WriteVectorFn, decltype(static_cast<WriteVectorFn>(&Externalizer::write))>);

static_assert(std::is_same_v<bool, decltype(std::declval<Externalizer&>().messageReadOnly())>);
static_assert(std::is_same_v<bool, decltype(std::declval<Externalizer&>().messageWriteOnly())>);
static_assert(std::is_same_v<bool, decltype(std::declval<Externalizer&>().supportsObjectRead())>);
static_assert(std::is_same_v<bool, decltype(std::declval<Externalizer&>().supportsObjectWrite())>);
static_assert(std::is_same_v<std::string, decltype(std::declval<Externalizer&>().getCalApiVersion())>);
static_assert(std::is_same_v<std::string, decltype(std::declval<Externalizer&>().getEncoding())>);
static_assert(std::is_same_v<std::string, decltype(std::declval<Externalizer&>().getSchemaVersion())>);
static_assert(std::is_same_v<std::string, decltype(std::declval<Externalizer&>().getVendorVersion())>);
static_assert(std::is_same_v<std::string, decltype(std::declval<Externalizer&>().getVendor())>);

using GetExternalizerFn = Externalizer* (Loader::*)(const std::string&, const std::string&, const std::string&);
using DestroyExternalizerFn = void (Loader::*)(Externalizer*);
static_assert(std::is_same_v<GetExternalizerFn, decltype(&Loader::getExternalizer)>);
static_assert(std::is_same_v<DestroyExternalizerFn, decltype(&Loader::destroyExternalizer)>);

using GetLoaderFn = Loader* (*)();
using DestroyLoaderFn = void (*)(Loader*);
static_assert(std::is_same_v<GetLoaderFn, decltype(&uci_getExternalizerLoader)>);
static_assert(std::is_same_v<DestroyLoaderFn, decltype(&uci_destroyExternalizerLoader)>);
