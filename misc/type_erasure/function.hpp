#pragma once

#include <utility>

namespace my_std {

template <class>
class function {};

template <class R, class... Args>
class function<R(Args...)> {
private:
    struct vtable {
        R (*invoke) (void*, Args...);
    };

    template <class T>
    static constexpr vtable vtable_for = {
        [](void* p, Args... args) {
            return static_cast<T*>(p)->operator()(args...);
        }
    };

    const vtable* vt_;
    void* fn_;

public:
    template <class T>
    function(T fn) : vt_(&vtable_for<T>), fn_(new T(fn)) {}

    R operator()(Args... args) {
        return vt_->invoke(fn_, args...);
    }  
};

}