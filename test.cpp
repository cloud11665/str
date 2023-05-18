#include <stdio.h>
#include <assert.h>
#include "str.hpp"
using namespace std::literals;

void test_pointer()
{
    Str128 b = "foo";
    auto func = [](Str* s){
        s->append("bar");
    };
    func(&b);
    assert(b == "foobar");
}

void test_append_nogrow()
{
    Str16 s = "aaaaaaaaaa";
    assert(s.append_nogrow("bbbbbb") == -1);
    assert(s == "aaaaaaaaaa");
}

void test_append()
{
    Str s = Str::ref("asdasdasd");
    assert(!s.owned());
    s += "aaa";
    assert(s.owned());
}

void test_shrink()
{
    Str s = Str::ref("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    int cap1 = s.capacity();
    s.set("smaller"); // allocates
    int cap2 = s.capacity();
    assert(cap1 > cap2);
    s.shrink_to_fit(); // doesn't change anything
    int cap3 = s.capacity();
    assert(cap2 == cap3);
}

int main() {
    test_pointer();
    test_append_nogrow();
    test_append();
    test_shrink();
}
