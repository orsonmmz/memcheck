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

#include "memcheck.hpp"
#include <cassert>

// tracked class
class foo
{
public:
    foo()
    {
        memcheck<foo>::get().created(this);
    }

    ~foo()
    {
        memcheck<foo>::get().destroyed(this);
    }
};

// to make stack traces more interesting, we need to create/destroy
// objects in functions other than main()
foo* create_foo()
{
    return new foo();
}

void destroy_foo(foo* a)
{
    delete a;
}

int main()
{
    foo* a;             // uninitialized on purpose
    foo* b = nullptr;   // this one will be leaked

    // both variables are not initialized yet
    std::cout << "a exists: " << memcheck<foo>::get().exists(a) << std::endl;
    assert(memcheck<foo>::get().exists(a) == false);
    assert(memcheck<foo>::get().exists(b) == false);

    // both variables are initialized
    a = create_foo();
    b = create_foo();
    std::cout << "a exists: " << memcheck<foo>::get().exists(a) << std::endl;
    assert(memcheck<foo>::get().exists(a) == true);
    assert(memcheck<foo>::get().exists(b) == true);

    // destroy one of them
    destroy_foo(a);
    // b is not destroyed
    std::cout << "a exists: " << memcheck<foo>::get().exists(a) << std::endl;
    assert(memcheck<foo>::get().exists(a) == false);
    assert(memcheck<foo>::get().exists(b) == true);


    std::cout << std::endl;

    // show where variable 'a' was created
    memcheck<foo>::get().show_create(a);
    std::cout << std::endl;

    // show where variable 'a' was destroyed
    memcheck<foo>::get().show_destroy(a);
    std::cout << std::endl;

    // show all valid objects of 'foo' type
    memcheck<foo>::get().show_objs();
    std::cout << std::endl;

    return 0;
}
