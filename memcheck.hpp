/*
 * memcheck - C++ debug utility for tracking objects lifetime
 *
 * Copyright (C) 2017 Maciej Suminski <orson@orson.net.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MEMCHECK_H
#define MEMCHECK_H

#include <dlfcn.h>
#include <execinfo.h>
#include <cxxabi.h>

#include <cassert>
#include <sstream>
#include <iostream>
#include <array>
#include <map>

// the following class comes from:
// https://en.wikibooks.org/wiki/Linux_Applications_Debugging_Techniques/The_call_stack
class call_stack
{
public:
    static const int depth = 40;
    typedef std::array<void *, depth> stack_t;

    class const_iterator;
    class frame
    {
    public:
        frame(void *addr = 0)
                : _addr(0)
                , _binary_name(0)
                , _func_name(0)
                , _demangled_func_name(0)
                , _delta_sign('+')
                , _delta(0L)
                , _source_file_name(0)
                , _line_number(0)
                , _dladdr_ret(false)
        {
            resolve(addr);
        }

        // frame(stack_t::iterator& it) : frame(*it) {} //C++0x
        frame(stack_t::const_iterator const& it)
                : _addr(0)
                , _binary_name(0)
                , _func_name(0)
                , _demangled_func_name(0)
                , _delta_sign('+')
                , _delta(0L)
                , _source_file_name(0)
                , _line_number(0)
                , _dladdr_ret(false)
        {
            resolve(*it);
        }

        frame(frame const& other)
        {
            resolve(other._addr);
        }

        frame& operator=(frame const& other)
        {
            if (this != &other) {
                resolve(other._addr);
            }
            return *this;
        }

        ~frame()
        {
            resolve(0);
        }

        __attribute__((noinline))
        std::string as_string() const
        {
            std::ostringstream s;
            s << "[" << std::hex << _addr << "] "
              << demangled_function()
              << " (" << binary_file() << _delta_sign << "0x" << std::hex << _delta << ")"
              << " in " << source_file() << ":" << line_number();
            return s.str();
        }

        __attribute__((noinline))
        const void* addr() const               { return _addr; }

        __attribute__((noinline))
        const char* binary_file() const        { return safe(_binary_name); }

        __attribute__((noinline))
        const char* function() const           { return safe(_func_name); }

        __attribute__((noinline))
        const char* demangled_function() const { return safe(_demangled_func_name); }

        __attribute__((noinline))
        char        delta_sign() const         { return _delta_sign; }

        __attribute__((noinline))
        long        delta() const              { return _delta; }

        __attribute__((noinline))
        const char* source_file() const        { return safe(_source_file_name); }

        __attribute__((noinline))
        int         line_number() const        { return  _line_number; }

    private:

        const char* safe(const char* p) const { return p ? p : "??"; }

        friend class const_iterator; // To call resolve()

        void resolve(const void* addr)
        {
            if (_addr == addr)
                return;

            _addr = addr;
            _dladdr_ret = false;
            _binary_name = 0;
            _func_name = 0;
            if (_demangled_func_name) {
                free(_demangled_func_name);
                _demangled_func_name = 0;
            }
            _delta_sign = '+';
            _delta = 0L;
            _source_file_name = 0;
            _line_number = 0;

            if (!_addr)
                return;

            _dladdr_ret = (::dladdr(_addr, &_info) != 0);
            if (_dladdr_ret)
            {
                _binary_name = safe(_info.dli_fname);
                _func_name   = safe(_info.dli_sname);
                _delta_sign  = (_addr >=  _info.dli_saddr) ? '+' : '-';
                _delta = ::labs(static_cast<const char *>(_addr) - static_cast<const char *>(_info.dli_saddr));

                 int status = 0;
                 _demangled_func_name = abi::__cxa_demangle(_func_name, 0, 0, &status);
            }
        }

    private:

        const void* _addr;
        const char* _binary_name;
        const char* _func_name;
        char*       _demangled_func_name;
        char        _delta_sign;
        long        _delta;
        const char* _source_file_name; //TODO: libbfd
        int         _line_number;

        Dl_info     _info;
        bool        _dladdr_ret;
    }; //frame


    class const_iterator
            : public std::iterator<std::bidirectional_iterator_tag, ptrdiff_t>
    {
    public:

        const_iterator(stack_t::const_iterator const& it)
                : _frame(it)
                , _it(it)
        {}

        bool operator==(const const_iterator& other) const
        {
            return _frame.addr() == other._frame.addr();
        }

        bool operator!=(const const_iterator& x) const
        {
            return !(*this == x);
        }

        const frame& operator*() const
        {
            return _frame;
        }
        const frame* operator->() const
        {
            return &_frame;
        }

        const_iterator& operator++()
        {
            ++_it;
            _frame.resolve(*_it);
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator tmp = *this;
            ++_it;
            _frame.resolve(*_it);
            return tmp;
        }

        const_iterator& operator--()
        {
            --_it;
            _frame.resolve(*_it);
            return *this;
        }
        const_iterator operator--(int)
        {
            const_iterator tmp = *this;
            --_it;
            _frame.resolve(*_it);
            return tmp;
        }

    private:
        const_iterator();

        frame                    _frame;
        stack_t::const_iterator  _it;
    }; //const_iterator


    public:
    call_stack() : _num_frames(0)
    {
        _num_frames = ::backtrace(_stack.data(), depth);
        assert(_num_frames >= 0 && _num_frames <= depth);
    }

    std::string as_string()
    {
        std::string s;
        const_iterator itEnd = end();
        for (const_iterator it = begin(); it != itEnd; ++it) {
            s += it->as_string();
            s += "\n";
        }
        return s;
    }

    virtual ~call_stack()
    {
    }

    const_iterator begin() const { return _stack.cbegin(); }
    const_iterator end() const   { return stack_t::const_iterator(&_stack[_num_frames]); }

private:
    stack_t  _stack;
    int      _num_frames;
};

template<typename T>
class memcheck
{
private:
    // an entry storing stack traces of construction & destruction of an object
    struct obj_info
    {
        obj_info() :
            create_trace(nullptr), destroy_trace(nullptr)
        {
        }

        call_stack* create_trace;
        call_stack* destroy_trace;
    };

public:
    static memcheck<T>& get()
    {
        // it has to be created on the heap so it is not destroyed at the
        // end, as you may want to check whether everything was deleted
        // if memcheck is the only leak, then you write good software
        static memcheck<T>* inst = nullptr;

        if(!inst)
            inst = new memcheck<T>();

        return *inst;
    }

    __attribute__((noinline))
    bool created(const T* obj)
    {
        assert(obj);

        if(!obj)
            return false;

        obj_info& info = entries[obj];

        // the object is either created for the first time or
        // it has been created and destroyed already
        assert((!info.create_trace && !info.destroy_trace)
                || (info.create_trace && info.destroy_trace));

        info.create_trace = new call_stack();
        info.destroy_trace = nullptr;   // mark as not yet destroyed
        return true;
    }

    __attribute__((noinline))
    bool destroyed(const T* obj)
    {
        assert(obj);

        if(!obj)
            return false;

        obj_info& info = entries[obj];

        // the object is either created for the first time or
        // it has been created and destroyed already
        assert(info.create_trace && !info.destroy_trace);

        info.destroy_trace = new call_stack();
        return true;
    }

    __attribute__((noinline))
    bool exists(const T* obj) const
    {
        auto it = entries.find(obj);

        if(it == entries.end())
            return false;       // never created

        const obj_info& info = it->second;

        // check if it has been created but not yet destroyed
        return (info.create_trace && !info.destroy_trace);
    }

    __attribute__((noinline))
    void show_create(const T* obj) const
    {
        assert(obj);
        auto it = entries.find(obj);
        call_stack* st = nullptr;

        if(it != entries.end() && (st = it->second.create_trace))
        {
            std::cout << "construction stack trace for " << obj << std::endl;
            std::cout << st->as_string();
            return;
        }

        std::cerr << obj << "has not been created" << std::endl;
    }

    __attribute__((noinline))
    void show_destroy(const T* obj) const
    {
        assert(obj);
        auto it = entries.find(obj);
        call_stack* st = nullptr;

        if(it != entries.end() && (st = it->second.destroy_trace))
        {
            std::cout << "destruction stack trace for " << obj << std::endl;
            std::cout << st->as_string();
            return;
        }

        std::cerr << obj << "has not been destroyed" << std::endl;
    }

    __attribute__((noinline))
    void show_objs(bool show_stack = false) const
    {
        std::cout << "existing objects:" << std::endl;

        for(const auto& info : entries)
        {
            const T* obj = info.first;

            if(exists(obj))
            {
                std::cout << obj << std::endl;

                if(show_stack)
                    show_create(obj);
            }
        }
    }

    std::map<const T*, obj_info> entries;
};

#endif /* MEMCHECK_H */
