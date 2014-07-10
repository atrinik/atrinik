/*
 * Copyright © 2007 -11 Anthony Williams
 * Copyright © 2011 -12 Vicente J. Botet Escriba
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

template <typename Lockable> class strict_lock {
public:
    typedef Lockable lockable_type;

    explicit strict_lock(lockable_type& obj) : obj_(obj)
    {
        obj.lock(); // locks on construction
    }
    strict_lock() = delete;
    strict_lock(strict_lock const&) = delete;
    strict_lock& operator=(strict_lock const&) = delete;

    // unlocks on destruction

    ~strict_lock()
    {
        obj_.unlock();
    }

    bool owns_lock(Lockable* mtx) const
    {
        return mtx == &obj_;
    }
private:
    lockable_type& obj_;
};
