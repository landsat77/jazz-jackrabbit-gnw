/*
 * C++ runtime of the homebrew (replaces sdk/src/gw_core_cxx_support.cpp,
 * filtered out of the link by the Makefile): operator new / delete use the
 * game heap (jj_alloc.c). The SDK version is a bump allocator without free,
 * and OpenJazz allocates and frees objects all the time.
 */
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include "jj_platform.h"

// Zeroed: the pictures are matched by their pixels (jj_bake.c), and some
// decoders leave parts of their buffers unwritten (short data): they must
// be the same on the PC and on the console.
static void *alloc(size_t s)
{
    void *p = jj_calloc(1, s ? s : 1);
#ifndef JJ_DEVICE
    // harness: the C++ runtime allocates before the game heap exists
    if (!p && jj_heap_size() == 0)
        p = (std::malloc)(s ? s : 1); // () : not the malloc macro of the glue
#endif
    if (!p && jj_heap_size() != 0) {
        static char msg[48];
        snprintf(msg, sizeof(msg), "out of memory (%u bytes)", (unsigned)s);
        jj_plat_fatal(msg);
    }
    return p;
}

#ifdef JJ_NEW_TRACE
#include <cstdio>
static void *traced(size_t s, void *ra)
{
    void *p = alloc(s);
    if (s >= 2048 && jj_heap_size())
        fprintf(stderr, "NEW %zu %p used %zu\n", s, ra, jj_heap_used());
    return p;
}
void *operator new(size_t s) { return traced(s, __builtin_return_address(0)); }
void *operator new[](size_t s) { return traced(s, __builtin_return_address(0)); }
#else
void *operator new(size_t s) { return alloc(s); }
void *operator new[](size_t s) { return alloc(s); }
#endif
void operator delete(void *p) noexcept { jj_free(p); }
void operator delete[](void *p) noexcept { jj_free(p); }
void operator delete(void *p, size_t) noexcept { jj_free(p); }
void operator delete[](void *p, size_t) noexcept { jj_free(p); }

#ifdef JJ_DEVICE
extern "C" void abort(void);

extern "C" void __cxa_pure_virtual()
{
    jj_plat_fatal("pure virtual call");
}

extern "C" {
void *__dso_handle = (void *)&__dso_handle;
}

extern "C" int __cxa_atexit(void (*)(void *), void *, void *)
{
    return 0;
}

// The few libstdc++ pieces OpenJazz uses (std::list, std::vector),
// without linking libstdc++ (its error paths pull std::string & co).
#include <list>
namespace std {
void __throw_length_error(const char *) { jj_plat_fatal("length_error"); }
void __throw_bad_alloc() { jj_plat_fatal("out of memory (bad_alloc)"); }
void __throw_bad_array_new_length() { jj_plat_fatal("bad_array_new_length"); }
namespace __detail {
void _List_node_base::_M_hook(_List_node_base *const position) noexcept
{
    this->_M_next = position;
    this->_M_prev = position->_M_prev;
    position->_M_prev->_M_next = this;
    position->_M_prev = this;
}
void _List_node_base::_M_unhook() noexcept
{
    _List_node_base *const next = this->_M_next;
    _List_node_base *const prev = this->_M_prev;
    prev->_M_next = next;
    next->_M_prev = prev;
}
}
}

// C++ exceptions come from libsupc++ (Makefile). Its default terminate
// handler prints with stdio and the demangler: replaced (not linked).
namespace __gnu_cxx {
void __verbose_terminate_handler() { jj_plat_fatal("uncaught C++ exception"); }
}
#endif
