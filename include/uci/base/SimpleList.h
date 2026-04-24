#pragma once

#include "Accessor.h"
#include "accessorType.h"
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace uci {
namespace base {

template <typename T, uci::base::accessorType::AccessorType V>
class SimpleList : public uci::base::Accessor {
public:
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = typename std::vector<T>::iterator;
    using const_iterator  = typename std::vector<T>::const_iterator;

    static constexpr size_type MAXIMUM_LENGTH = std::numeric_limits<size_type>::max();

    AccessorType getAccessorType() const noexcept override { return V; }
    const std::string& typeName() const override {
        static const std::string name{"list"};
        return name;
    }

    virtual size_type size()     const noexcept = 0;
    virtual bool      empty()    const noexcept = 0;
    virtual size_type capacity() const noexcept = 0;

    virtual void reserve(size_type n) = 0;
    virtual void resize(size_type n, uci::base::accessorType::AccessorType = V) = 0;
    virtual void pop_back() noexcept = 0;
    virtual void clear() noexcept = 0;

    virtual void push_back(const T& v) = 0;

    virtual reference       operator[](size_type i) = 0;
    virtual const_reference operator[](size_type i) const = 0;
    virtual reference       at(size_type i) = 0;
    virtual const_reference at(size_type i) const = 0;

    virtual iterator       begin() = 0;
    virtual const_iterator begin() const = 0;
    virtual iterator       end() = 0;
    virtual const_iterator end() const = 0;

    virtual size_type getLength() const noexcept = 0;
    virtual size_type getMinimumLength() const noexcept = 0;
    virtual size_type getMaximumLength() const noexcept = 0;
    virtual size_type max_size() const noexcept = 0;
    virtual size_type min_size() const noexcept = 0;

protected:
    SimpleList() = default;
    SimpleList(const SimpleList&) = default;
    SimpleList& operator=(const SimpleList&) = default;
    ~SimpleList() override = default;
};

} // namespace base
} // namespace uci
