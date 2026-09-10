#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace npc::policy::detail {
template<class T> class PersistentMap {
    struct Node;
    using Ptr = std::shared_ptr<const Node>;
    struct Node {
        std::string key;
        std::shared_ptr<const T> value;
        Ptr left, right;
        int height;
        std::size_t size;
        Node(std::string k, std::shared_ptr<const T> v, Ptr l, Ptr r)
            : key(std::move(k)), value(std::move(v)), left(std::move(l)), right(std::move(r)),
              height(1 + std::max(h(left), h(right))), size(1 + n(left) + n(right)) {}
    };
    Ptr root_;
    static int h(const Ptr& p) noexcept { return p ? p->height : 0; }
    static std::size_t n(const Ptr& p) noexcept { return p ? p->size : 0; }
    static Ptr node(const std::string& k, std::shared_ptr<const T> v, Ptr l, Ptr r) {
        return std::make_shared<const Node>(k, std::move(v), std::move(l), std::move(r));
    }
    static Ptr rotate_left(const Ptr& a) {
        const auto& b = a->right;
        return node(b->key, b->value, node(a->key, a->value, a->left, b->left), b->right);
    }
    static Ptr rotate_right(const Ptr& a) {
        const auto& b = a->left;
        return node(b->key, b->value, b->left, node(a->key, a->value, b->right, a->right));
    }
    static Ptr balance(Ptr p) {
        if (h(p->left) - h(p->right) > 1) {
            if (h(p->left->left) < h(p->left->right))
                p = node(p->key, p->value, rotate_left(p->left), p->right);
            return rotate_right(p);
        }
        if (h(p->right) - h(p->left) > 1) {
            if (h(p->right->right) < h(p->right->left))
                p = node(p->key, p->value, p->left, rotate_right(p->right));
            return rotate_left(p);
        }
        return p;
    }
    static Ptr put(const Ptr& p, const std::string& key, const std::shared_ptr<const T>& value) {
        if (!p) return node(key, value, {}, {});
        if (key < p->key) return balance(node(p->key, p->value, put(p->left, key, value), p->right));
        if (key > p->key) return balance(node(p->key, p->value, p->left, put(p->right, key, value)));
        return node(key, value, p->left, p->right);
    }
    static Ptr remove(const Ptr& p, const std::string& key) {
        if (!p) return {};
        if (key < p->key) return balance(node(p->key, p->value, remove(p->left, key), p->right));
        if (key > p->key) return balance(node(p->key, p->value, p->left, remove(p->right, key)));
        if (!p->left) return p->right;
        if (!p->right) return p->left;
        auto successor = p->right;
        while (successor->left) successor = successor->left;
        return balance(node(successor->key, successor->value, p->left, remove(p->right, successor->key)));
    }
    template<class F> static void walk(const Ptr& p, bool reverse, std::size_t& remaining, F& fn) {
        if (!p || remaining == 0) return;
        walk(reverse ? p->right : p->left, reverse, remaining, fn);
        if (remaining == 0) return;
        fn(p->key, *p->value); --remaining;
        walk(reverse ? p->left : p->right, reverse, remaining, fn);
    }
public:
    const T* find(const std::string& key) const noexcept {
        auto p = root_;
        while (p) {
            if (key == p->key) return p->value.get();
            p = key < p->key ? p->left : p->right;
        }
        return nullptr;
    }
    std::size_t size() const noexcept { return n(root_); }
    int height() const noexcept { return h(root_); }
    void set(const std::string& key, T value) { root_ = put(root_, key, std::make_shared<const T>(std::move(value))); }
    void erase(const std::string& key) { root_ = remove(root_, key); }
    template<class F> void visit(std::size_t limit, F fn, bool reverse = false) const {
        walk(root_, reverse, limit, fn);
    }
};
} // namespace npc::policy::detail
