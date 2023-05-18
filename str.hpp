/*
# Str v0.40
## Simple C++ string type with an optional local buffer, by Omar Cornut, cloud11665
https://github.com/cloud11665/str

### License:
    This software is in the public domain. Where that dedication is not
    recognized, you are granted a perpetual, irrevocable license to copy,
    distribute, and modify this file as you see fit.

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

*/

/*
 CHANGELOG
  0.40 - Added libfmt support, reworked api.
  0.32 - added owned() accessor.
  0.31 - fixed various warnings.
  0.30 - turned into a single header file, removed Str.cpp.
  0.29 - fixed bug when calling reserve on non-owned strings (ie. when using StrRef or set_ref), and fixed <string> include.
  0.28 - breaking change: replaced Str32 by Str30 to avoid collision with Str32 from MacTypes.h .
  0.27 - added STR_API and basic .natvis file.
  0.26 - fixed set(cont char* src, const char* src_end) writing null terminator to the wrong position.
  0.25 - allow set(const char* NULL) or operator= NULL to clear the string. note that set() from range or other types are not allowed.
  0.24 - allow set_ref(const char* NULL) to clear the string. include fixes for linux.
  0.23 - added append(char). added append_from(int idx, XXX) functions. fixed some compilers warnings.
  0.22 - documentation improvements, comments. fixes for some compilers.
  0.21 - added StrXXXf() constructor to construct directly from a format string.
*/

#pragma once

//TODO: rework configuration.
//-------------------------------------------------------------------------
// CONFIGURATION
//-------------------------------------------------------------------------

#ifndef STR_MEMALLOC
#define STR_MEMALLOC  malloc
#include <stdlib.h>
#endif

#ifndef STR_MEMFREE
#define STR_MEMFREE   free
#include <stdlib.h>
#endif

#ifndef STR_ASSERT
#define STR_ASSERT    assert
#include <assert.h>
#endif

#ifndef STR_API
#define STR_API
#endif

#include <string.h>   // for strlen, strcmp, memcpy, etc.
#include <fmt/format.h>
#include <string_view>
#include <compare>

//-------------------------------------------------------------------------
// HEADERS
//-------------------------------------------------------------------------

// This is the base class that you can pass around
// Footprint is 16-bytes
class STR_API Str
{
private:
    // TODO: there must be a better way to do this, right ?
    char*   m_data;               // Point to LocalBuf() or heap allocated
    unsigned int    m_size : 24;
    unsigned int    m_local_size : 8;
    unsigned int    m_capacity : 24;
    unsigned int    m_owned : 1;  // Set when we have ownership of the pointed data (most common, unless using set_ref() method or StrRef constructor)

public:
    inline char*        c_str()                                 { return m_data; }
    inline std::string_view view() const                        { return static_cast<std::string_view>(*this); }
    inline bool         empty() const                           { return m_size == 0; }
    inline int          size() const                            { return m_size; }
    inline int          capacity() const                        { return m_capacity; }
    inline bool         owned() const                           { return m_owned ? true : false; }

    inline void         set_ref(std::string_view s);
    int                 append(std::string_view s);
    int                 append_nogrow(std::string_view s);
    
    template<typename... Args> int  setf(fmt::format_string<Args...> fm, Args&&... args);
    template<typename... Args> int  setf_nogrow(fmt::format_string<Args...> fm, Args&&... args);
    template<typename... Args> int  appendf(fmt::format_string<Args...> fm, Args&&... args);
    template<typename... Args> int  appendf_nogrow(fmt::format_string<Args...> fm, Args&&... args);

    void                clear();
    void                reserve(int cap);
    void                reserve_discard(int cap);
    void                shrink_to_fit();

    inline char&        operator[](size_t i)                     { STR_ASSERT(-m_size < i && i < m_size); return m_data[i + (i < 0 ? m_size : 0)]; }
    inline char         operator[](size_t i) const               { STR_ASSERT(-m_size < i && i < m_size); return m_data[i + (i < 0 ? m_size : 0)]; }
    explicit operator   std::string_view() const                 { return std::string_view{m_data, m_size}; } // Don't know if we should keep this.

    inline Str();
    inline Str(std::string_view s)                               { m_local_size = 0; m_owned = 0; set(s); } // m_owned gets reset in call to set().
    inline Str(const char* s)                                    { m_local_size = 0; m_owned = 0; set(s); }
    inline void         set(std::string_view src);
    inline Str&         operator=(std::string_view rhs)          { set(rhs); return *this; }
    inline Str&         operator+=(std::string_view rhs)         { append(rhs); return *this; }
    inline bool         operator==(std::string_view rhs) const   { return view() == rhs; }
    inline auto         operator<=>(std::string_view rhs) const  { return view() <=> rhs; }

    static inline Str   ref(std::string_view s);

    // Destructor for all variants
    inline ~Str()
    {
        if (m_owned && !is_using_local_buf())
            STR_MEMFREE(m_data);
    }

    static char*        EmptyBuffer;

protected:
    inline char*        local_buf()                             { return (char*)this + sizeof(Str); }
    inline const char*  local_buf() const                       { return (char*)this + sizeof(Str); }
    inline bool         is_using_local_buf() const              { return m_data == local_buf(); }

    // Constructor for StrXXX variants with local buffer
    Str(int local_buf_size)
    {
        STR_ASSERT(local_buf_size <= 256);
        m_data = local_buf();
        m_data[0] = '\0';
        m_capacity = local_buf_size;
        m_local_size = local_buf_size;
        m_size = 0;
        m_owned = 1;
    }
};

Str::Str()
{
    m_data = EmptyBuffer;      // Shared READ-ONLY initial buffer for 0 capacity
    m_capacity = 0;
    m_local_size = 0;
    m_size = 0;
    m_owned = 0;
}

void    Str::set(std::string_view src)
{
    int buf_len = src.size() + 1;
    reserve_discard(buf_len);
    memcpy(m_data, src.data(), (size_t)buf_len);
    m_owned = 1;
    m_size = src.size();
}

inline void Str::set_ref(std::string_view s)
{
    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);
    m_data = const_cast<char*>(s.data());
    m_size = s.size();
    m_capacity = s.size();
    m_owned = 0;
}

inline Str Str::ref(std::string_view s)
{
    Str tmp;
    tmp.set_ref(s);
    return tmp;
}


template<size_t LOCALBUFFSIZE>
class StrN : public Str
{
private:
    char m_local_buf[LOCALBUFFSIZE];
public:
    StrN() : Str(LOCALBUFFSIZE) {}
    StrN(std::string_view s) : Str(LOCALBUFFSIZE) { set(s); }
    StrN(const char* s) : Str(LOCALBUFFSIZE) { set(s); }
    StrN& operator=(std::string_view s) { set(s); return *this; }
    StrN& operator=(const char* s) { set(s); return *this; }
};

// Disable PVS-Studio warning V730: Not all members of a class are initialized inside the constructor (local_buf is not initialized and that is fine)
// -V:STR_DEFINETYPE:730

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"         // warning : private field 'local_buf' is not used
#endif

// Declaring types for common sizes here
using Str16 = StrN<16>;
using Str32 = StrN<32>;
using Str64 = StrN<64>;
using Str128 = StrN<128>;
using Str256 = StrN<256>;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

//-------------------------------------------------------------------------
// IMPLEMENTATION
//-------------------------------------------------------------------------

// Static empty buffer we can point to for empty strings
// Pointing to a literal increases the like-hood of getting a crash if someone attempts to write in the empty string buffer.
char*   Str::EmptyBuffer = (char*)"\0NULL";

// Clear
void    Str::clear()
{
    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);
    if (m_local_size)
    {
        m_data = local_buf();
        m_data[0] = '\0';
        m_capacity = m_local_size;
        m_owned = 1;
    }
    else
    {
        m_data = EmptyBuffer;
        m_capacity = 0;
        m_owned = 0;
    }
    m_size = 0;
}

// Reserve memory, preserving the current of the buffer
void    Str::reserve(int new_capacity)
{
    if (new_capacity <= m_capacity)
        return;

    char* new_data;
    if (new_capacity < m_local_size) {
        // Disowned -> LocalBuf
        new_data = local_buf();
        new_capacity = m_local_size;
    } else {
        // Disowned or LocalBuf -> Heap
        new_data = (char*)STR_MEMALLOC(new_capacity);
    }

    memcpy(new_data, m_data, m_size);
    new_data[m_size] = 0;

    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);

    m_data = new_data;
    m_capacity = new_capacity;
    m_owned = 1;
}

// Reserve memory, discarding the current of the buffer (if we expect to be fully rewritten)
void    Str::reserve_discard(int new_capacity)
{
    if (m_owned && new_capacity <= m_capacity)
        return;

    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);

    if (new_capacity < m_local_size)
    {
        // Disowned -> LocalBuf
        m_data = local_buf();
        m_capacity = m_local_size;
    }
    else
    {
        // Disowned or LocalBuf -> Heap
        m_data = (char*)STR_MEMALLOC((size_t)new_capacity);
        m_capacity = new_capacity;
    }
    m_owned = 1;
}

void    Str::shrink_to_fit()
{
    if (!m_owned || is_using_local_buf())
        return;
    int new_capacity = m_size + 1;
    if (m_capacity <= new_capacity)
        return;

    char* new_data = (char*)STR_MEMALLOC((size_t)new_capacity);
    memcpy(new_data, m_data, (size_t)new_capacity);
    STR_MEMFREE(m_data);
    m_data = new_data;
    m_capacity = new_capacity;
}

template<typename... Args>
int     Str::setf(fmt::format_string<Args...> fm, Args&&... args)
{
    int len = fmt::formatted_size(fm, std::forward<Args>(args)...);
    if (m_capacity < len + 1)
        reserve_discard(len + 1);
    STR_ASSERT(m_owned);
    fmt::format_to_n(m_data, m_capacity, fm, std::forward<Args>(args)...);

    return len;
}

template<typename... Args>
int     Str::setf_nogrow(fmt::format_string<Args...> fm, Args&&... args)
{
    int len = fmt::formatted_size(fm, std::forward<Args>(args)...);
    if (m_capacity < len + 1)
        reserve_discard(len + 1);
    STR_ASSERT(m_owned);
    fmt::format_to_n(m_data, m_capacity, fm, std::forward<Args>(args)...);

    return len;
}

int     Str::append(std::string_view s)
{
    reserve(size() + s.size() + 1);
    memcpy(m_data + size(), s.data(), s.size());
    m_size += s.size();
    m_data[m_size] = 0;
    return s.size();
}

int     Str::append_nogrow(std::string_view s)
{
    if (m_capacity < m_size + s.size() + 1)
        return -1;
    int to_copy = std::min((int)s.size(), std::max(0, m_capacity - m_size - 1));
    strncpy(m_data + m_size, s.data(), to_copy);
    m_size += to_copy;
    m_data[m_size] = 0;
    return to_copy;
}

template<typename... Args>
int     Str::appendf(fmt::format_string<Args...> fm, Args&&... args)
{
    int len = fmt::formatted_size(fm, std::forward<Args>(args)...);
    if (m_capacity < len + 1)
        reserve(len + 1);
    STR_ASSERT(m_owned);
    fmt::format_to_n(m_data + m_size, m_capacity, fm, std::forward<Args>(args)...);
    m_size += len;
    m_data[m_size] = 0;
    return len;
}

template<typename... Args>
int     Str::appendf_nogrow(fmt::format_string<Args...> fm, Args&&... args)
{
    int len = fmt::formatted_size(fm, std::forward<Args>(args)...);
    if (m_capacity < len + 1)
        return -1;

    STR_ASSERT(m_owned);
    fmt::format_to_n(m_data + m_size, m_capacity, fm, std::forward<Args>(args)...);
    m_size += len;
    m_data[m_size] = 0;
    return len;
}
