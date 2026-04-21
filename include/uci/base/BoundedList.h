#pragma once

#include "Accessor.h"
#include "UCIException.h"
#include "accessorType.h"
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace uci {
namespace base {

template <typename T, uci::base::accessorType::AccessorType V>
class BoundedList : public uci::base::Accessor {
public:
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = typename std::vector<T>::iterator;
    using const_iterator  = typename std::vector<T>::const_iterator;

    static constexpr size_type UNBOUNDED_BOUND = std::numeric_limits<size_type>::max();

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
    void resize(size_type n, uci::base::accessorType::AccessorType = V) {
        if (n > maxOccurs_) throwUciException("BoundedList::resize exceeds maxOccurs=" << maxOccurs_);
        data_.resize(n);
    }
    void pop_back() noexcept { data_.pop_back(); }
    void clear()    noexcept { data_.clear(); }

    void push_back(const T& v) {
        if (data_.size() >= maxOccurs_)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << maxOccurs_);
        data_.push_back(v);
    }
    template <typename U>
    void push_back(U&& v) {
        if (data_.size() >= maxOccurs_)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << maxOccurs_);
        data_.push_back(std::forward<U>(v));
    }

    reference       operator[](size_type i)       { return data_[i]; }
    const_reference operator[](size_type i) const { return data_[i]; }
    reference       at(size_type i)               { return data_.at(i); }
    const_reference at(size_type i) const         { return data_.at(i); }

    iterator       begin()       { return data_.begin(); }
    const_iterator begin() const { return data_.begin(); }
    iterator       end()         { return data_.end(); }
    const_iterator end()   const { return data_.end(); }

    size_type getMinimumOccurs() const noexcept { return minOccurs_; }
    size_type getMaximumOccurs() const noexcept { return maxOccurs_; }
    size_type max_size()         const noexcept { return maxOccurs_; }
    size_type min_size()         const noexcept { return minOccurs_; }

protected:
    BoundedList(size_type minOccurs = 0, size_type maxOccurs = UNBOUNDED_BOUND)
        : minOccurs_(minOccurs), maxOccurs_(maxOccurs) {}
    BoundedList(const BoundedList&) = default;
    BoundedList& operator=(const BoundedList&) = default;
    ~BoundedList() override = default;

private:
    size_type minOccurs_{0};
    size_type maxOccurs_{UNBOUNDED_BOUND};
    std::vector<T> data_;
};

} // namespace base
} // namespace uci
