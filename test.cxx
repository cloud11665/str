#include <stdio.h>
#include "str.h"
using namespace std::literals;

int main() {
    Str128 a = "foobar";
    Str128 b = "";
    b = "asdasdasdasd";
    // printf("%d %d %d %s\n", a.size(), a.capacity(), a.owned(), a.c_str());
    // a.appendf("{2}{1}{0}", 1, 2, 3);
    // printf("%d %s\n", a.size(), a.c_str());
}