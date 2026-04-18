#pragma once

#include "UCIException.h"
#include <cstddef>
#include <limits>
#include <vector>

namespace uci {
namespace base {

template <typename T>
class SimpleList {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = typename std::vector<T>::iterator;
    using const_iterator  = typename std::vector<T>::const_iterator;

    size_type size() const     { return data_.size(); }
    bool empty() const         { return data_.empty(); }
    size_type capacity() const { return data_.capacity(); }

    void reserve(size_type n)  { data_.reserve(n); }
    void resize(size_type n)   { data_.resize(n); }
    void pop_back()            { data_.pop_back(); }
    void clear()               { data_.clear(); }

    void push_back(const T& v) { data_.push_back(v); }

    reference       operator[](size_type i)       { return data_[i]; }
    const_reference operator[](size_type i) const { return data_[i]; }
    reference       at(size_type i)               { return data_.at(i); }
    const_reference at(size_type i) const         { return data_.at(i); }

    iterator       begin()        { return data_.begin(); }
    const_iterator begin()  const { return data_.begin(); }
    iterator       end()          { return data_.end(); }
    const_iterator end()    const { return data_.end(); }

    size_type getLength() const        { return data_.size(); }
    size_type getMinimumLength() const { return minLength_; }
    size_type getMaximumLength() const { return maxLength_; }
    size_type max_size() const         { return maxLength_; }
    size_type min_size() const         { return minLength_; }

    SimpleList() = default;
    SimpleList(const SimpleList&) = default;
    SimpleList& operator=(const SimpleList&) = default;
    ~SimpleList() = default;

protected:
    size_type minLength_{0};
    size_type maxLength_{std::numeric_limits<size_type>::max()};

private:
    std::vector<T> data_;
};

} // namespace base
} // namespace uci
