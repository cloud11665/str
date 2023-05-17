// Str v0.32
// Simple C++ string type with an optional local buffer, by Omar Cornut
// https://github.com/ocornut/str

// LICENSE
//  This software is in the public domain. Where that dedication is not
//  recognized, you are granted a perpetual, irrevocable license to copy,
//  distribute, and modify this file as you see fit.

// USAGE
//  Include this file in whatever places need to refer to it.
//  In ONE .cpp file, write '#define STR_IMPLEMENTATION' before the #include of this file.
//  This expands out the actual implementation into that C/C++ file.


/*
- This isn't a fully featured string class.
- It is a simple, bearable replacement to std::string that isn't heap abusive nor bloated (can actually be debugged by humans).
- String are mutable. We don't maintain size so length() is not-constant time.
- Maximum string size currently limited to 2 MB (we allocate 21 bits to hold capacity).
- Local buffer size is currently limited to 1023 bytes (we allocate 10 bits to hold local buffer size).
- In "non-owned" mode for literals/reference we don't do any tracking/counting of references.
- Overhead is 8-bytes in 32-bits, 16-bytes in 64-bits (12 + alignment).
- This code hasn't been tested very much. it is probably incomplete or broken. Made it for my own use.

The idea is that you can provide an arbitrary sized local buffer if you expect string to fit
most of the time, and then you avoid using costly heap.

No local buffer, always use heap, sizeof()==8~16 (depends if your pointers are 32-bits or 64-bits)

   Str s = "hey";

With a local buffer of 16 bytes, sizeof() == 8~16 + 16 bytes.

   Str16 s = "filename.h"; // copy into local buffer
   Str16 s = "long_filename_not_very_long_but_longer_than_expected.h";   // use heap

With a local buffer of 256 bytes, sizeof() == 8~16 + 256 bytes.

   Str256 s = "long_filename_not_very_long_but_longer_than_expected.h";  // copy into local buffer

Common sizes are defined at the bottom of Str.h, you may define your own.

Functions:

   Str256 s;
   s.set("hello sailor");                   // set (copy)
   s.setf("%s/%s.tmp", folder, filename);   // set (w/format)
   s.append("hello");                       // append. cost a length() calculation!
   s.appendf("hello %d", 42);               // append (w/format). cost a length() calculation!
   s.set_ref("Hey!");                       // set (literal/reference, just copy pointer, no tracking)

Constructor helper for format string: add a trailing 'f' to the type. Underlying type is the same.

   Str256f filename("%s/%s.tmp", folder, filename);             // construct (w/format)
   fopen(Str256f("%s/%s.tmp, folder, filename).c_str(), "rb");  // construct (w/format), use as function param, destruct

Constructor helper for reference/literal:

   StrRef ref("literal");                   // copy pointer, no allocation, no string copy
   StrRef ref2(GetDebugName());             // copy pointer. no tracking of anything whatsoever, know what you are doing!

All StrXXX types derives from Str and instance hold the local buffer capacity. So you can pass e.g. Str256* to a function taking base type Str* and it will be functional.

   void MyFunc(Str& s) { s = "Hello"; }     // will use local buffer if available in Str instance

(Using a template e.g. Str<N> we could remove the m_local_size storage but it would make passing typed Str<> to functions tricky.
 Instead we don't use template so you can pass them around as the base type Str*. Also, templates are ugly.)
*/

/*
 CHANGELOG
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

#ifndef STR_INCLUDED
#define STR_INCLUDED

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
#include <fmt/format.h> // don't use varargs.
#include <string_view>
#include <compare>

//-------------------------------------------------------------------------
// HEADERS
//-------------------------------------------------------------------------

// This is the base class that you can pass around
// Footprint is 8-bytes (32-bits arch) or 16-bytes (64-bits arch)
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
    inline const char*  c_str() const                           { return m_data; }
    inline std::string_view view() const                           { return static_cast<std::string_view>(*this); }
    inline bool         empty() const                           { return m_size == 0; }
    inline int          size() const                            { return m_size; }
    inline int          capacity() const                        { return m_capacity; }
    inline bool         owned() const                           { return m_owned ? true : false; }

    inline void         set_ref(std::string_view s);
    int                 setf(std::string_view fmt, auto&& args...);
    int                 setf_nogrow(std::string_view fmt, auto&& args...);
    int                 append(char c);
    int                 append(std::string_view s);
    int                 appendf(std::string_view fmt, auto&& args...);

    void                clear();
    void                reserve(int cap);
    void                reserve_discard(int cap);
    void                shrink_to_fit();

    inline char&        operator[](size_t i)                    { return m_data[i]; }
    inline char         operator[](size_t i) const              { return m_data[i]; }
    explicit operator   std::string_view() const                { return std::string_view{m_data, m_size}; } // Don't know if we should keep this.

    inline Str();
    inline Str(std::string_view s);
    inline void         set(std::string_view src);
    inline Str&         operator=(std::string_view rhs)          { set(rhs); return *this; }
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

void    Str::set(std::string_view src)
{
    int buf_len = src.size();
    if (m_capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(m_data, src.data(), (size_t)buf_len);
    m_owned = 1;
}

inline void Str::set_ref(std::string_view s)
{
    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);
    m_data = const_cast<char*>(s.data());
    m_capacity = 0;
    m_owned = 0;
}

Str::Str()
{
    m_data = EmptyBuffer;      // Shared READ-ONLY initial buffer for 0 capacity
    m_capacity = 0;
    m_local_size = 0;
    m_owned = 0;
}

inline Str Str::ref(std::string_view s)
{
    Str tmp;
    tmp.set_ref(s);
    return tmp;
}


template<size_t LOCALBUFSIZE>
class StrN : public Str
{
private:
    char m_local_buf[LOCALBUFSIZE];
public:
    StrN() : Str(LOCALBUFSIZE) {}
    StrN(std::string_view s) : Str(LOCALBUFFSIZE) { set(s); }
    StrN& operator=(std::string_view s) { set(s); return *this; }
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

#endif // #ifndef STR_INCLUDED

//-------------------------------------------------------------------------
// IMPLEMENTATION
//-------------------------------------------------------------------------

#ifndef STR_IMPLEMENTATION


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

    // string in m_data might be longer than new_capacity if it wasn't owned, don't copy too much
#ifdef _MSC_VER
    strncpy_s(new_data, (size_t)new_capacity, m_data, (size_t)new_capacity - 1);
#else
    strncpy(new_data, m_data, (size_t)new_capacity - 1);
#endif
    new_data[new_capacity - 1] = 0;

    if (m_owned && !is_using_local_buf())
        STR_MEMFREE(m_data);

    m_data = new_data;
    m_capacity = new_capacity;
    m_owned = 1;
}

// Reserve memory, discarding the current of the buffer (if we expect to be fully rewritten)
void    Str::reserve_discard(int new_capacity)
{
    if (new_capacity <= m_capacity)
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
        m_data = (char*)STR_MEMALLOC((size_t)new_capacity * sizeof(char));
        m_capacity = new_capacity;
    }
    m_owned = 1;
}

void    Str::shrink_to_fit()
{
    if (!m_owned || is_using_local_buf())
        return;
    int new_capacity = size() + 1;
    if (m_capacity <= new_capacity)
        return;

    char* new_data = (char*)STR_MEMALLOC((size_t)new_capacity * sizeof(char));
    memcpy(new_data, m_data, (size_t)new_capacity);
    STR_MEMFREE(m_data);
    m_data = new_data;
    m_capacity = new_capacity;
}

int     Str::setf(std::string_view fmt, auto&& args...)
{
    int len = fmt::formatted_size(fmt, std::forward<decltype(args...)>(args...));
    if (m_capacity < len + 1)
        reserve_discard(len + 1);
    STR_ASSERT(m_owned);
    fmt::format_to_n(m_data, m_capacity, fmt, std::forward<decltype(args...)>(args...));

    return len;
}

int     Str::setf_nogrow(std::string_view fmt, auto&& args...)
{
    STR_ASSERT(m_owned);
    int len = fmt::formatted_size(fmt, std::forward<decltype(args...)>(args...));
    auto res = fmt::format_to_n(m_data, m_capacity, fmt, std::forward<decltype(args...)>(args...));

    // return len;
    // STR_ASSERT(m_owned);

    // if (m_capacity == 0)
    //     return 0;

    // int w = vsnprintf(m_data, (size_t)m_capacity, fmt, args);
    // m_data[m_capacity - 1] = 0;
    // m_owned = 1;
    // return (w == -1) ? m_capacity - 1 : w;
}

int     Str::append_from(int idx, char c)
{
    int add_len = 1;
    if (m_capacity < idx + add_len + 1)
        reserve(idx + add_len + 1);
    m_data[idx] = c;
    m_data[idx + add_len] = 0;
    STR_ASSERT(m_owned);
    return add_len;
}

int     Str::append_from(int idx, const char* s, const char* s_end)
{
    if (!s_end)
        s_end = s + strlen(s);
    int add_len = (int)(s_end - s);
    if (m_capacity < idx + add_len + 1)
        reserve(idx + add_len + 1);
    memcpy(m_data + idx, (const void*)s, (size_t)add_len);
    m_data[idx + add_len] = 0; // Our source data isn't necessarily zero-terminated
    STR_ASSERT(m_owned);
    return add_len;
}

// FIXME: merge setfv() and appendfv()?
int     Str::appendfv_from(int idx, const char* fmt, va_list args)
{
    // Needed for portability on platforms where va_list are passed by reference and modified by functions
    va_list args2;
    va_copy(args2, args);

    // MSVC returns -1 on overflow when writing, which forces us to do two passes
    // FIXME-OPT: Find a way around that.
#ifdef _MSC_VER
    int add_len = vsnprintf(NULL, 0, fmt, args);
    STR_ASSERT(add_len >= 0);

    if (m_capacity < idx + add_len + 1)
        reserve(idx + add_len + 1);
    add_len = vsnprintf(m_data + idx, add_len + 1, fmt, args2);
#else
    // First try
    int add_len = vsnprintf(m_owned ? m_data + idx : NULL, m_owned ? (size_t)(m_capacity - idx) : 0, fmt, args);
    STR_ASSERT(add_len >= 0);

    if (m_capacity < idx + add_len + 1)
    {
        reserve(idx + add_len + 1);
        add_len = vsnprintf(m_data + idx, (size_t)add_len + 1, fmt, args2);
    }
#endif

    STR_ASSERT(m_owned);
    return add_len;
}

int     Str::appendf_from(int idx, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = appendfv_from(idx, fmt, args);
    va_end(args);
    return len;
}

int     Str::append(char c)
{
    int cur_len = length();
    return append_from(cur_len, c);
}

int     Str::append(const char* s, const char* s_end)
{
    int cur_len = length();
    return append_from(cur_len, s, s_end);
}

int     Str::appendfv(const char* fmt, va_list args)
{
    int cur_len = length();
    return appendfv_from(cur_len, fmt, args);
}

int     Str::appendf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = appendfv(fmt, args);
    va_end(args);
    return len;
}

#endif // #define STR_IMPLEMENTATION

//-------------------------------------------------------------------------
