# Str v0.40
## Simple C++ string type with an optional local buffer, by Omar Cornut, cloud11665
https://github.com/cloud11665/str

### License:
&nbsp;&nbsp;&nbsp;&nbsp;This software is in the public domain. Where that dedication is not  
&nbsp;&nbsp;&nbsp;&nbsp;recognized, you are granted a perpetual, irrevocable license to copy,  
&nbsp;&nbsp;&nbsp;&nbsp;distribute, and modify this file as you see fit.

### Usage:
&nbsp;&nbsp;&nbsp;&nbsp;Include this file in whatever places need to refer to it.

### Note:
- This isn't a fully featured string class.
- It is a simple, bearable replacement to std::string that isn't heap abusive nor bloated (can actually be debugged by humans).
- Local buffer size is currently limited to 1023 bytes (we allocate 10 bits to hold local buffer size).
- In "non-owned" mode for literals/reference we don't do any tracking/counting of references.

The main idea is that you can provide an arbitrary sized local buffer if you expect string to fit most of the time, and then you avoid using costly heap.
```cpp
// No local buffer, always use heap, sizeof()==16
    Str s1 = "hey"; // allocates
    Str s2 = Str::ref("hey"); // doesn't allocate
// With a local buffer of 16 bytes, sizeof() == 16 + 16 bytes.
    Str16 s = "filename.h"; // copy into local buffer
    Str16 s = "long_filename_not_very_long_but_longer_than_expected.h";   // use heap
// With a local buffer of 256 bytes, sizeof() == 16 + 256 bytes.
    Str256 s = "long_filename_not_very_long_but_longer_than_expected.h";  // copy into local buffer
// Common sizes are defined at the bottom of Str.h, you may define your own.
```

## Functions:
```cpp
    Str256 s;
    s.set("hello sailor");                   // set (copy)
    s.setf("{}/{}.tmp", folder, filename);   // set (w/format)
    s.append("hello");                       // append.
    s.appendf("hello {}", 42);               // append (w/format).
    s.set_ref("Hey!");                       // set (literal/reference, just copy pointer, no tracking)
```

Constructor helper for reference/literal:
```cpp
    Str ref = Str::ref("literal");           // copy pointer, no allocation, no string copy
    Str ref = Str::ref(GetDebugName());      // copy pointer. no tracking of anything whatsoever, know what you are doing!
```

All StrN types derives from Str and instance hold the local buffer capacity. So you can pass e.g. Str256* to a function taking base type Str* and it will be functional:
```cpp
    void MyFunc(Str* s) { *s = "Hello"; }    // will use local buffer if available in Str instance
```

## Testing the code:
    g++ -std=c++20 -g test.cpp -o test -lfmt
    valgrind ./test