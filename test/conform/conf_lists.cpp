// Compile-only conformance: SimpleList and BoundedList API shape (OMSC-SPC-008 RevK).

#include "uci/base/Accessor.h"
#include "uci/base/BoundedList.h"
#include "uci/base/SimpleList.h"
#include "uci/base/accessorType.h"

#include <cstddef>
#include <type_traits>

namespace at = uci::base::accessorType;

struct ElementAccessor : uci::base::Accessor {
    AccessorType getAccessorType() const noexcept override { return at::ACCESSOR_TYPE_COMPLEX; }
    void reset() override {}
    const std::string& typeName() const override {
        static const std::string name{"ElementAccessor"};
        return name;
    }

protected:
    ~ElementAccessor() override = default;
};

using Simple = uci::base::SimpleList<ElementAccessor, at::ACCESSOR_TYPE_COMPLEX>;
using Bounded = uci::base::BoundedList<ElementAccessor, at::ACCESSOR_TYPE_COMPLEX>;

static_assert(std::is_base_of_v<uci::base::Accessor, Simple>,
    "SimpleList<T, V> must publicly inherit uci::base::Accessor");
static_assert(std::is_base_of_v<uci::base::Accessor, Bounded>,
    "BoundedList<T, V> must publicly inherit uci::base::Accessor");

static_assert(std::is_same_v<Simple::size_type, std::size_t>);
static_assert(std::is_same_v<Simple::reference, ElementAccessor&>);
static_assert(std::is_same_v<Simple::const_reference, const ElementAccessor&>);
static_assert(std::is_same_v<Bounded::size_type, std::size_t>);
static_assert(std::is_same_v<Bounded::reference, ElementAccessor&>);
static_assert(std::is_same_v<Bounded::const_reference, const ElementAccessor&>);

static_assert(std::is_same_v<decltype(Simple::MAXIMUM_LENGTH), const Simple::size_type>,
    "SimpleList must define public static const size_type MAXIMUM_LENGTH");
static_assert(std::is_same_v<decltype(Bounded::UNBOUNDED_BOUND), const Bounded::size_type>,
    "BoundedList must define public static const size_type UNBOUNDED_BOUND");

using SimpleResizeFn = void (Simple::*)(Simple::size_type, at::AccessorType);
using BoundedResizeFn = void (Bounded::*)(Bounded::size_type, at::AccessorType);
using SimplePushBackFn = void (Simple::*)(Simple::const_reference);
using BoundedPushBackFn = void (Bounded::*)(Bounded::const_reference);

static_assert(std::is_same_v<SimpleResizeFn, decltype(&Simple::resize)>,
    "SimpleList::resize must accept size_type and optional AccessorType");
static_assert(std::is_same_v<BoundedResizeFn, decltype(&Bounded::resize)>,
    "BoundedList::resize must accept size_type and optional AccessorType");
static_assert(std::is_same_v<SimplePushBackFn, decltype(static_cast<SimplePushBackFn>(&Simple::push_back))>);
static_assert(std::is_same_v<BoundedPushBackFn, decltype(static_cast<BoundedPushBackFn>(&Bounded::push_back))>);

static_assert(noexcept(std::declval<const Simple&>().size()));
static_assert(noexcept(std::declval<const Simple&>().max_size()));
static_assert(noexcept(std::declval<const Simple&>().min_size()));
static_assert(noexcept(std::declval<const Simple&>().capacity()));
static_assert(noexcept(std::declval<const Simple&>().empty()));
static_assert(noexcept(std::declval<Simple&>().pop_back()));
static_assert(noexcept(std::declval<Simple&>().clear()));
static_assert(noexcept(std::declval<const Simple&>().getMaximumLength()));
static_assert(noexcept(std::declval<const Simple&>().getMinimumLength()));
static_assert(noexcept(std::declval<const Simple&>().getLength()));

static_assert(noexcept(std::declval<const Bounded&>().size()));
static_assert(noexcept(std::declval<const Bounded&>().max_size()));
static_assert(noexcept(std::declval<const Bounded&>().min_size()));
static_assert(noexcept(std::declval<const Bounded&>().capacity()));
static_assert(noexcept(std::declval<const Bounded&>().empty()));
static_assert(noexcept(std::declval<Bounded&>().clear()));
static_assert(noexcept(std::declval<const Bounded&>().getMaximumOccurs()));
static_assert(noexcept(std::declval<const Bounded&>().getMinimumOccurs()));

static_assert(!std::is_constructible_v<Simple>, "SimpleList default constructor must be protected");
static_assert(!std::is_copy_constructible_v<Simple>, "SimpleList copy constructor must be protected");
static_assert(!std::is_assignable_v<Simple&, const Simple&>, "SimpleList assignment must be protected");
static_assert(!std::is_destructible_v<Simple>, "SimpleList destructor must be protected");

static_assert(!std::is_constructible_v<Bounded>, "BoundedList default constructor must be protected");
static_assert(!std::is_copy_constructible_v<Bounded>, "BoundedList copy constructor must be protected");
static_assert(!std::is_assignable_v<Bounded&, const Bounded&>, "BoundedList assignment must be protected");
static_assert(!std::is_destructible_v<Bounded>, "BoundedList destructor must be protected");
