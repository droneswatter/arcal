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
    void reset() override { data_.clear(); }
    const std::string& typeName() const override {
        static const std::string name{"list"};
        return name;
    }

    size_type size()     const noexcept { return data_.size(); }
    bool      empty()    const noexcept { return data_.empty(); }
    size_type capacity() const noexcept { return data_.capacity(); }

    void reserve(size_type n) { data_.reserve(n); }
    void resize(size_type n, uci::base::accessorType::AccessorType = V) { data_.resize(n); }
    void pop_back() noexcept  { data_.pop_back(); }
    void clear()    noexcept  { data_.clear(); }

    void push_back(const T& v) { data_.push_back(v); }

    reference       operator[](size_type i)       { return data_[i]; }
    const_reference operator[](size_type i) const { return data_[i]; }
    reference       at(size_type i)               { return data_.at(i); }
    const_reference at(size_type i) const         { return data_.at(i); }

    iterator       begin()       { return data_.begin(); }
    const_iterator begin() const { return data_.begin(); }
    iterator       end()         { return data_.end(); }
    const_iterator end()   const { return data_.end(); }

    size_type getLength()        const noexcept { return data_.size(); }
    size_type getMinimumLength() const noexcept { return 0; }
    size_type getMaximumLength() const noexcept { return MAXIMUM_LENGTH; }
    size_type max_size()         const noexcept { return MAXIMUM_LENGTH; }
    size_type min_size()         const noexcept { return 0; }

protected:
    SimpleList() = default;
    SimpleList(const SimpleList&) = default;
    SimpleList& operator=(const SimpleList&) = default;
    ~SimpleList() override = default;

private:
    std::vector<T> data_;
};

} // namespace base
} // namespace uci
