#pragma once

#include "Accessor.h"
#include "UCIException.h"
#include "accessorType.h"
#include <cstddef>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace uci {
namespace base {

template <typename T, uci::base::accessorType::AccessorType V>
class BoundedList : public uci::base::Accessor {
public:
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;

    class iterator {
        friend class const_iterator;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator(BoundedList& list, size_type index) : list_(&list), index_(index) {}
        reference operator*() const { return (*list_)[index_]; }
        pointer operator->() const { return &(*list_)[index_]; }
        iterator& operator++() { ++index_; return *this; }
        iterator operator++(int) { auto copy = *this; ++(*this); return copy; }
        bool operator==(const iterator& rhs) const { return list_ == rhs.list_ && index_ == rhs.index_; }
        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

    private:
        BoundedList* list_;
        size_type index_;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator(const BoundedList& list, size_type index) : list_(&list), index_(index) {}
        const_iterator(const iterator& it) : list_(it.list_), index_(it.index_) {}
        reference operator*() const { return (*list_)[index_]; }
        pointer operator->() const { return &(*list_)[index_]; }
        const_iterator& operator++() { ++index_; return *this; }
        const_iterator operator++(int) { auto copy = *this; ++(*this); return copy; }
        bool operator==(const const_iterator& rhs) const { return list_ == rhs.list_ && index_ == rhs.index_; }
        bool operator!=(const const_iterator& rhs) const { return !(*this == rhs); }

    private:
        const BoundedList* list_;
        size_type index_;
    };

    static constexpr size_type UNBOUNDED_BOUND = std::numeric_limits<size_type>::max();

    AccessorType getAccessorType() const noexcept override { return V; }
    const std::string& typeName() const override {
        static const std::string name{"list"};
        return name;
    }

    virtual size_type size() const noexcept = 0;
    virtual bool empty() const noexcept = 0;
    virtual size_type capacity() const noexcept = 0;

    virtual void reserve(size_type n) = 0;
    virtual void resize(size_type n, uci::base::accessorType::AccessorType = V) = 0;
    virtual void pop_back() noexcept = 0;
    virtual void clear() noexcept = 0;

    virtual void push_back(const T& v) = 0;
    template <typename U,
              typename std::enable_if<
                  !std::is_lvalue_reference<U&&>::value &&
                  !std::is_base_of<T, typename std::decay<U>::type>::value &&
                  std::is_assignable<reference, U&&>::value,
                  int>::type = 0>
    void push_back(U&& v) {
        T item{};
        item = std::move(v);
        push_back(item);
    }
    template <typename U,
              typename std::enable_if<
                  !std::is_same<typename std::decay<U>::type, T>::value &&
                  !std::is_base_of<T, typename std::decay<U>::type>::value &&
                  std::is_assignable<reference, const U&>::value,
                  int>::type = 0>
    void push_back(const U& v) {
        T item{};
        item = v;
        push_back(item);
    }

    virtual reference operator[](size_type i) = 0;
    virtual const_reference operator[](size_type i) const = 0;
    virtual reference at(size_type i) = 0;
    virtual const_reference at(size_type i) const = 0;

    virtual iterator begin() = 0;
    virtual const_iterator begin() const = 0;
    virtual iterator end() = 0;
    virtual const_iterator end() const = 0;

    virtual size_type getMinimumOccurs() const noexcept = 0;
    virtual size_type getMaximumOccurs() const noexcept = 0;
    virtual size_type max_size() const noexcept = 0;
    virtual size_type min_size() const noexcept = 0;

protected:
    BoundedList() = default;
    BoundedList(const BoundedList&) = default;
    BoundedList& operator=(const BoundedList&) = default;
    ~BoundedList() override = default;
};

} // namespace base
} // namespace uci
