#pragma once

#include "SimpleList.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace uci {
namespace base {

// Concrete, publicly constructible SimpleList for use in generated type headers.
// SimpleList itself has a protected lifecycle to enforce spec conformance.
template <typename T, uci::base::accessorType::AccessorType V, typename StorageT = T>
class SimpleListImpl : public SimpleList<T, V> {
public:
    using typename SimpleList<T, V>::size_type;
    using typename SimpleList<T, V>::reference;
    using typename SimpleList<T, V>::const_reference;
    using typename SimpleList<T, V>::iterator;
    using typename SimpleList<T, V>::const_iterator;

    SimpleListImpl() = default;
    SimpleListImpl(const SimpleListImpl&) = default;
    SimpleListImpl& operator=(const SimpleListImpl&) = default;
    ~SimpleListImpl() = default;

    void reset() override { data_.clear(); }

    size_type size()     const noexcept override { return data_.size(); }
    bool      empty()    const noexcept override { return data_.empty(); }
    size_type capacity() const noexcept override { return data_.capacity(); }

    void reserve(size_type n) override { data_.reserve(n); }
    void resize(size_type n, uci::base::accessorType::AccessorType = V) override { data_.resize(n); }
    void pop_back() noexcept override { data_.pop_back(); }
    void clear() noexcept override { data_.clear(); }

    void push_back(const T& v) override {
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
        data_.push_back(std::forward<U>(v));
    }

    reference       operator[](size_type i) override { return data_[i]; }
    const_reference operator[](size_type i) const override { return data_[i]; }
    reference       at(size_type i) override { return data_.at(i); }
    const_reference at(size_type i) const override { return data_.at(i); }

    iterator       begin() override { return iterator(*this, 0); }
    const_iterator begin() const override { return const_iterator(*this, 0); }
    iterator       end() override { return iterator(*this, data_.size()); }
    const_iterator end() const override { return const_iterator(*this, data_.size()); }

    size_type getLength() const noexcept override { return data_.size(); }
    size_type getMinimumLength() const noexcept override { return 0; }
    size_type getMaximumLength() const noexcept override { return SimpleList<T, V>::MAXIMUM_LENGTH; }
    size_type max_size() const noexcept override { return SimpleList<T, V>::MAXIMUM_LENGTH; }
    size_type min_size() const noexcept override { return 0; }

private:
    std::vector<StorageT> data_;
};

} // namespace base
} // namespace uci
