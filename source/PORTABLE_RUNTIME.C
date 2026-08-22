#include "TYPES.H"
#include "FILE_LIB.H"
#include "UTIL_LIB.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdint.h>
#include "dos_diag.h"

int32 _argc;
char **_argv;

static void (*shutdown_function)(void);
static uint32_t watcom_randnext = 1u;
static char *strtok_next;

#define DUKE_ATEXIT_MAX 16
static void (*duke_atexit_handlers[DUKE_ATEXIT_MAX])(void);
static int duke_atexit_count;

int atexit(void (*function)(void))
{
    if (!function || duke_atexit_count >= DUKE_ATEXIT_MAX)
        return -1;
    duke_atexit_handlers[duke_atexit_count++] = function;
    return 0;
}

void Shutdown(void);

void _fini(void *ctx)
{
#if DIAG
    dos_diag_set(0xD3F10000u | ((uint32_t)duke_atexit_count & 0xffu));
#endif
    (void)ctx;

    Shutdown();

    while (duke_atexit_count > 0)
    {
        void (*fn)(void) = duke_atexit_handlers[--duke_atexit_count];
#if DIAG
        dos_diag_set(0xD3F10100u | ((uint32_t)duke_atexit_count & 0xffu));
#endif
        if (fn)
            fn();
#if DIAG
        dos_diag_set(0xD3F10200u | ((uint32_t)duke_atexit_count & 0xffu));
#endif
    }
}

void *_fmemcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

void RegisterShutdownFunction(void (*shutdown)(void))
{
    shutdown_function = shutdown;
}

void Shutdown(void)
{
    void (*fn)(void) = shutdown_function;
    shutdown_function = 0;
    if (fn)
        fn();
}

void Error(char *error, ...)
{
    va_list args;

    va_start(args, error);
    vprintf(error, args);
    va_end(args);
    Shutdown();
    exit(1);
}

char CheckParm(char *check)
{
    int32 i;

    for (i = 1; i < _argc; ++i)
        if (strcmpi(check, _argv[i]) == 0)
            return (char)i;
    return 0;
}

void *SafeMalloc(int32 size)
{
    void *ptr = malloc((size_t)size);
    if (!ptr)
        Error("SafeMalloc: failed on allocation of %ld bytes\n", (long)size);
    return ptr;
}

void SafeFree(void *ptr)
{
    free(ptr);
}

void SafeRealloc(void **ptr, int32 newsize)
{
    void *newptr = realloc(*ptr, (size_t)newsize);
    if (!newptr && newsize != 0)
        Error("SafeRealloc: failed on allocation of %ld bytes\n", (long)newsize);
    *ptr = newptr;
}

int32 IntelLong(int32 value)
{
    return value;
}

int32 SafeOpenRead(const char *filename, int32 filetype)
{
    int handle;
    (void)filetype;

    handle = open(filename, O_RDONLY | O_BINARY);
    if (handle < 0)
        Error("SafeOpenRead: unable to open %s\n", filename);
    return handle;
}

boolean SafeFileExists(const char *filename)
{
    return access(filename, 0) == 0;
}

void SafeRead(int32 handle, void *buffer, int32 count)
{
    int done = read(handle, buffer, (unsigned int)count);
    if (done != count)
        Error("SafeRead: read %d of %ld bytes\n", done, (long)count);
}

long atol(const char *s)
{
    long value = 0;
    int sign = 1;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '\f' || *s == '\v')
        ++s;
    if (*s == '+' || *s == '-')
    {
        if (*s == '-')
            sign = -1;
        ++s;
    }
    while (*s >= '0' && *s <= '9')
    {
        value = value * 10 + (*s - '0');
        ++s;
    }
    return sign < 0 ? -value : value;
}

void srand(unsigned int seed)
{
    watcom_randnext = seed;
}

int rand(void)
{
    watcom_randnext = watcom_randnext * 1103515245u + 12345u;
    return (int)((watcom_randnext >> 16) & 0x7fffu);
}

int isalnum(int c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'));
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

int stricmp(const char *a, const char *b)
{
    return strcmpi(a, b);
}

char *strtok(char *s, const char *delim)
{
    char *start;
    const char *d;

    if (s)
        strtok_next = s;
    else if (!strtok_next)
        return 0;

    s = strtok_next;
    for (;; ++s)
    {
        if (*s == '\0')
        {
            strtok_next = 0;
            return 0;
        }
        for (d = delim; *d && *d != *s; ++d)
            ;
        if (*d == '\0')
            break;
    }

    start = s;
    while (*s)
    {
        for (d = delim; *d && *d != *s; ++d)
            ;
        if (*d)
        {
            *s = '\0';
            strtok_next = s + 1;
            return start;
        }
        ++s;
    }

    strtok_next = 0;
    return start;
}

char *ltoa(long value, char *buffer, int radix)
{
    char tmp[34];
    unsigned long u;
    unsigned int i = 0;
    unsigned int out = 0;
    int negative = 0;

    if (radix < 2 || radix > 36)
    {
        buffer[0] = '\0';
        return buffer;
    }

    if (value < 0 && radix == 10)
    {
        negative = 1;
        u = 0ul - (unsigned long)value;
    }
    else
        u = (unsigned long)value;

    do
    {
        unsigned int digit = (unsigned int)(u % (unsigned long)radix);
        tmp[i++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        u /= (unsigned long)radix;
    } while (u);

    if (negative)
        buffer[out++] = '-';
    while (i)
        buffer[out++] = tmp[--i];
    buffer[out] = '\0';
    return buffer;
}

int unlink(const char *filename)
{
    return remove(filename);
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}
