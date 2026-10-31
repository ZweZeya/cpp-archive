#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace my_std {

template <typename T>
class list {
private:
    struct list_node_base {
        list_node_base* next{nullptr};
        list_node_base* prev{nullptr};
    };

    struct list_node : list_node_base {
        T val;
        list_node(T val) : val(std::move(val)) {}
    };

    std::size_t size_{0};
    list_node_base* sentinel_{nullptr};

    void ensure_sentinel() {
        if (!sentinel_) {
            sentinel_ = new list_node_base;
            sentinel_->next = sentinel_;
            sentinel_->prev = sentinel_;
        }
    }

    static void destroy(list_node_base* sentinel, std::size_t n) {
        if (!sentinel) return;

        list_node_base* curr_node = sentinel->next;
        for (std::size_t i = 0; i < n; i++) {
            list_node* tmp = static_cast<list_node*>(curr_node);
            curr_node = curr_node->next;
            delete tmp;
        }

        delete sentinel;
    }

    void link_before(list_node_base* pos, list_node_base* node) {
        assert(pos && node);

        list_node_base* before_pos = pos->prev;
        node->prev = before_pos;
        before_pos->next = node;
        node->next = pos;
        pos->prev = node;
        size_++;
    }

    void unlink(list_node_base* node) {
        assert(node && node != sentinel_);

        list_node_base* prev_node = node->prev;
        list_node_base* next_node = node->next;

        prev_node->next = next_node;
        next_node->prev = prev_node;
        size_--;
    }

    static list_node_base* copy(list_node_base* other_sentinel, std::size_t other_size) {
        if (!other_sentinel) return nullptr;

        std::size_t size = 0;
        list_node_base* sentinel = new list_node_base;
        list_node_base* curr_node = sentinel;
        list_node_base* itr_node = other_sentinel->next;
        
        try {
            while (size < other_size) {
                list_node* new_node = new list_node(static_cast<list_node*>(itr_node)->val);

                new_node->prev = curr_node;
                curr_node->next = new_node;
                curr_node = curr_node->next;
                itr_node = itr_node->next;
                size++;
            } 
        } catch(...) {
            destroy(sentinel, size);
            throw;
        }

        curr_node->next = sentinel;
        sentinel->prev = curr_node;
        return sentinel;
    }

public:
    template <bool IsConst>
    class basic_iterator {
    private:
        using node_base_ptr = std::conditional_t<IsConst, const list_node_base*, list_node_base*>;
        using node_ptr = std::conditional_t<IsConst, const list_node*, list_node*>;

        node_base_ptr ptr{nullptr};

        explicit basic_iterator(node_base_ptr p) noexcept : ptr(p) {}

        friend class list;
        friend class basic_iterator<!IsConst>;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;

        basic_iterator() noexcept = default;

        template <bool OtherConst> requires(IsConst && !OtherConst)
        basic_iterator(const basic_iterator<OtherConst>& other) noexcept : ptr(other.ptr) {}

        basic_iterator& operator++() {
            ptr = ptr->next;
            return *this;
        }

        basic_iterator& operator--() {
            ptr = ptr->prev;
            return *this;
        }

        basic_iterator operator++(int) {
            basic_iterator tmp(*this);
            ptr = ptr->next;
            return tmp;
        }

        basic_iterator operator--(int) {
            basic_iterator tmp(*this);
            ptr = ptr->prev;
            return tmp;
        }

        reference operator*() const {
            return static_cast<node_ptr>(ptr)->val;
        }

        pointer operator->() const {
            return &(static_cast<node_ptr>(ptr)->val);
        }

        friend bool operator==(const basic_iterator& a, const basic_iterator& b) noexcept {
            return a.ptr == b.ptr;
        }
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

private:
    list_node_base* to_node(const_iterator pos) {
        list_node_base* pos_node = const_cast<list_node_base*>(pos.ptr);
        return pos_node ? pos_node : sentinel_;
    }

public:
    list() noexcept = default;

    list(const list& other) {
        sentinel_ = copy(other.sentinel_, other.size_);
        size_ = other.size_;
    }

    list(list&& other) noexcept : size_(other.size_), sentinel_(other.sentinel_) {
        other.size_ = 0;
        other.sentinel_ = nullptr;
    }

    list& operator=(const list& other) {
        if (this == &other) return *this;

        list_node_base* new_sentinel = copy(other.sentinel_, other.size_);
        destroy(sentinel_, size_);
        sentinel_ = new_sentinel;
        size_ = other.size_;

        return *this;
    }

    list& operator=(list&& other) noexcept {
        if (this == &other) return *this;

        list_node_base* old_sentinel = sentinel_;
        std::size_t old_size = size_;

        sentinel_ = other.sentinel_;
        size_ = other.size_;

        other.sentinel_ = nullptr;
        other.size_ = 0;

        destroy(old_sentinel, old_size);

        return *this;
    }

    ~list() {
        destroy(sentinel_, size_);
    }

    void push_back(const T& val) {
        ensure_sentinel();
        link_before(sentinel_, new list_node(val));
    }

    void push_back(T&& val) {
        ensure_sentinel();
        link_before(sentinel_, new list_node(std::move(val)));

    }

    void push_front(const T& val) {
        ensure_sentinel();
        link_before(sentinel_->next, new list_node(val));
    }

    void push_front(T&& val) {
        ensure_sentinel();
        link_before(sentinel_->next, new list_node(std::move(val)));

    }

    iterator insert(const_iterator pos, const T& val) {
        ensure_sentinel();
        list_node_base* node = new list_node(val);
        link_before(to_node(pos), node);
        return iterator(node);
    }

    iterator insert(const_iterator pos, T&& val) {
        ensure_sentinel();
        list_node_base* node = new list_node(std::move(val));
        link_before(to_node(pos), node);
        return iterator(node);
    }

    iterator erase(iterator pos) {
        list_node_base* node = pos.ptr;
        list_node_base* tmp = node->next;
        unlink(node);
        delete static_cast<list_node*>(node);
        return iterator(tmp);
    }

    void pop_back() {
        assert(size_ > 0 && sentinel_);

        list_node_base* tail = sentinel_->prev;
        unlink(tail);
        delete static_cast<list_node*>(tail);
    }

    void pop_front() {
        assert(size_ > 0 && sentinel_);

        list_node_base* head = sentinel_->next;
        unlink(head);
        delete static_cast<list_node*>(head);
    }

    bool empty() const noexcept { return size_ == 0; }

    std::size_t size() const noexcept { return size_; }

    void clear() {
        destroy(sentinel_, size_);
        sentinel_ = nullptr;
        size_ = 0;
    }

    iterator begin() { 
        return sentinel_ ? iterator(sentinel_->next) : iterator(nullptr); 
    }

    iterator end() { 
        return iterator(sentinel_); 
    }

    const_iterator begin() const { 
        return cbegin();
    }

    const_iterator end() const { 
        return cend();
    }

    const_iterator cbegin() const { 
        return sentinel_ ? const_iterator(sentinel_->next) : const_iterator(nullptr); 
    }

    const_iterator cend() const { 
        return const_iterator(sentinel_); 
    }
};

}
