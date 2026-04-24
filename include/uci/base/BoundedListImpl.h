#pragma once

#include "BoundedList.h"

#include "UCIException.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace uci {
namespace base {

// Concrete, publicly constructible BoundedList for use in generated type headers.
// BoundedList itself has a protected lifecycle to enforce spec conformance.
// MinOccurs/MaxOccurs are compile-time bounds forwarded to the base constructor.
template <typename T, uci::base::accessorType::AccessorType V,
          std::size_t MinOccurs = 0,
          std::size_t MaxOccurs = BoundedList<T, V>::UNBOUNDED_BOUND,
          typename StorageT = T>
class BoundedListImpl : public BoundedList<T, V> {
public:
    using typename BoundedList<T, V>::size_type;
    using typename BoundedList<T, V>::reference;
    using typename BoundedList<T, V>::const_reference;
    using typename BoundedList<T, V>::iterator;
    using typename BoundedList<T, V>::const_iterator;

    BoundedListImpl() = default;
    BoundedListImpl(const BoundedListImpl&) = default;
    BoundedListImpl& operator=(const BoundedListImpl&) = default;
    ~BoundedListImpl() = default;

    void reset() override { data_.clear(); }

    size_type size() const noexcept override { return data_.size(); }
    bool empty() const noexcept override { return data_.empty(); }
    size_type capacity() const noexcept override { return data_.capacity(); }

    void reserve(size_type n) override { data_.reserve(n); }
    void resize(size_type n, uci::base::accessorType::AccessorType = V) override {
        if (n > maxOccurs_) throwUciException("BoundedList::resize exceeds maxOccurs=" << maxOccurs_);
        data_.resize(n);
    }
    void pop_back() noexcept override { data_.pop_back(); }
    void clear() noexcept override { data_.clear(); }

    void push_back(const T& v) override {
        if (data_.size() >= maxOccurs_)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << maxOccurs_);
        if constexpr (std::is_same_v<T, StorageT>) {
            data_.push_back(v);
        } else {
            StorageT item;
            item.copy(v);
            data_.push_back(std::move(item));
        }
    }
    template <typename U>
    void push_back(U&& v) {
        if (data_.size() >= maxOccurs_)
            throwUciException("BoundedList::push_back exceeds maxOccurs=" << maxOccurs_);
        data_.push_back(std::forward<U>(v));
    }

    reference operator[](size_type i) override { return data_[i]; }
    const_reference operator[](size_type i) const override { return data_[i]; }
    reference at(size_type i) override { return data_.at(i); }
    const_reference at(size_type i) const override { return data_.at(i); }

    iterator begin() override { return iterator(*this, 0); }
    const_iterator begin() const override { return const_iterator(*this, 0); }
    iterator end() override { return iterator(*this, data_.size()); }
    const_iterator end() const override { return const_iterator(*this, data_.size()); }

    size_type getMinimumOccurs() const noexcept override { return minOccurs_; }
    size_type getMaximumOccurs() const noexcept override { return maxOccurs_; }
    size_type max_size() const noexcept override { return maxOccurs_; }
    size_type min_size() const noexcept override { return minOccurs_; }

private:
    size_type minOccurs_{MinOccurs};
    size_type maxOccurs_{MaxOccurs};
    std::vector<StorageT> data_;
};

} // namespace base
} // namespace uci
