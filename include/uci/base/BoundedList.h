#pragma once

#include "UCIException.h"
#include <cstddef>
#include <limits>
#include <vector>

namespace uci {
namespace base {

template <typename T, std::size_t MinOccurs = 0, std::size_t MaxOccurs = std::numeric_limits<std::size_t>::max()>
class BoundedList {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = typename std::vector<T>::iterator;
    using const_iterator  = typename std::vector<T>::const_iterator;

    static constexpr size_type MIN_OCCURS = MinOccurs;
    static constexpr size_type MAX_OCCURS = MaxOccurs;

    size_type size() const     { return data_.size(); }
    bool empty() const         { return data_.empty(); }
    size_type capacity() const { return data_.capacity(); }

    void reserve(size_type n)  { data_.reserve(n); }
    void pop_back()            { data_.pop_back(); }
    void clear()               { data_.clear(); }

    void resize(size_type n) {
        if (n > MAX_OCCURS) throwUciException("BoundedList::resize exceeds maxOccurs=" << MAX_OCCURS);
        data_.resize(n);
    }

    void push_back(const T& v) {
        if (data_.size() >= MAX_OCCURS)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << MAX_OCCURS);
        data_.push_back(v);
    }

    template <typename U>
    void push_back(U&& v) {
        if (data_.size() >= MAX_OCCURS)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << MAX_OCCURS);
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

    size_type getMinimumOccurs() const { return MIN_OCCURS; }
    size_type getMaximumOccurs() const { return MAX_OCCURS; }
    size_type max_size() const         { return MAX_OCCURS; }
    size_type min_size() const         { return MIN_OCCURS; }

    BoundedList() = default;
    BoundedList(const BoundedList&) = default;
    BoundedList& operator=(const BoundedList&) = default;
    ~BoundedList() = default;

private:
    std::vector<T> data_;
};

} // namespace base
} // namespace uci
