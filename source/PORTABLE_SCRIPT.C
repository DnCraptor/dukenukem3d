#include "TYPES.H"
#include "SCRIPLIB.H"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PS_MAX_SCRIPTS   4
#define PS_MAX_SECTIONS 32
#define PS_MAX_ENTRIES 256
#define PS_NAME_LEN      64
#define PS_VALUE_LEN    192

typedef struct
{
    char name[PS_NAME_LEN];
} ps_section_t;

typedef struct
{
    int section;
    char name[PS_NAME_LEN];
    char value[PS_VALUE_LEN];
} ps_entry_t;

typedef struct
{
    int used;
    int section_count;
    int entry_count;
    ps_section_t sections[PS_MAX_SECTIONS];
    ps_entry_t entries[PS_MAX_ENTRIES];
} ps_script_t;

static ps_script_t ps_scripts[PS_MAX_SCRIPTS];

static int ps_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int ps_icmp(const char *a, const char *b)
{
    unsigned char ca, cb;
    for (;;)
    {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb || !ca) return (int)ca - (int)cb;
    }
}

static char *ps_findchar(const char *s, int c)
{
    while (*s)
    {
        if ((unsigned char)*s == (unsigned char)c) return (char *)s;
        ++s;
    }
    return NULL;
}

static char *ps_ltrim(char *s)
{
    while (*s && ps_space((unsigned char)*s)) ++s;
    return s;
}

static void ps_rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && ps_space((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void ps_copy(char *dst, size_t dstsz, const char *src)
{
    if (!dstsz) return;
    strncpy(dst, src ? src : "", dstsz - 1);
    dst[dstsz - 1] = '\0';
}

static int ps_find_section(ps_script_t *sc, const char *name)
{
    int i;
    for (i = 0; i < sc->section_count; ++i)
        if (!ps_icmp(sc->sections[i].name, name)) return i;
    return -1;
}

static int ps_add_section(ps_script_t *sc, const char *name)
{
    int i = ps_find_section(sc, name);
    if (i >= 0) return i;
    if (sc->section_count >= PS_MAX_SECTIONS) return -1;
    i = sc->section_count++;
    ps_copy(sc->sections[i].name, sizeof(sc->sections[i].name), name);
    return i;
}

static int ps_find_entry(ps_script_t *sc, int section, const char *name)
{
    int i;
    for (i = 0; i < sc->entry_count; ++i)
        if (sc->entries[i].section == section &&
            !ps_icmp(sc->entries[i].name, name)) return i;
    return -1;
}

static int ps_set_entry(ps_script_t *sc, const char *section, const char *name,
                        const char *value)
{
    int si = ps_add_section(sc, section);
    int ei;
    if (si < 0) return -1;
    ei = ps_find_entry(sc, si, name);
    if (ei < 0)
    {
        if (sc->entry_count >= PS_MAX_ENTRIES) return -1;
        ei = sc->entry_count++;
        sc->entries[ei].section = si;
        ps_copy(sc->entries[ei].name, sizeof(sc->entries[ei].name), name);
    }
    ps_copy(sc->entries[ei].value, sizeof(sc->entries[ei].value), value);
    return ei;
}

static ps_script_t *ps_handle(int32 handle)
{
    if (handle <= 0 || handle > PS_MAX_SCRIPTS) return NULL;
    if (!ps_scripts[handle - 1].used) return NULL;
    return &ps_scripts[handle - 1];
}

static char *ps_value(ps_script_t *sc, const char *section, const char *entry)
{
    int si = ps_find_section(sc, section);
    int ei;
    if (si < 0) return NULL;
    ei = ps_find_entry(sc, si, entry);
    return ei < 0 ? NULL : sc->entries[ei].value;
}

static void ps_unquote_one(const char *src, char *dst)
{
    const char *p = src;
    char quote = 0;
    size_t n = 0;

    while (*p && ps_space((unsigned char)*p)) ++p;
    if (*p == '"' || *p == '\'') quote = *p++;
    while (*p)
    {
        if (quote && *p == quote) break;
        if (!quote && (*p == ';' || *p == '#')) break;
        dst[n++] = *p++;
        if (n == PS_VALUE_LEN - 1) break;
    }
    dst[n] = '\0';
    ps_rtrim(dst);
}

int32 SCRIPT_Load(char *filename)
{
    FILE *f;
    long length;
    char *data, *line, *next;
    int slot, current = -1;
    ps_script_t *sc;

    for (slot = 0; slot < PS_MAX_SCRIPTS; ++slot)
        if (!ps_scripts[slot].used) break;
    if (slot == PS_MAX_SCRIPTS) return -1;

    f = fopen(filename, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    rewind(f);
    if (length < 0)
    {
        fclose(f);
        return -1;
    }
    data = (char *)malloc((size_t)length + 1);
    if (!data)
    {
        fclose(f);
        return -1;
    }
    if (fread(data, 1, (size_t)length, f) != (size_t)length)
    {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    data[length] = '\0';

    sc = &ps_scripts[slot];
    memset(sc, 0, sizeof(*sc));
    sc->used = 1;

    line = data;
    while (*line)
    {
        char *p, *eq;
        next = ps_findchar(line, '\n');
        if (next) *next++ = '\0';
        else next = line + strlen(line);
        p = ps_ltrim(line);
        ps_rtrim(p);
        if (*p && *p != ';' && *p != '#')
        {
            if (*p == '[')
            {
                char *close = ps_findchar(p + 1, ']');
                if (close)
                {
                    *close = '\0';
                    current = ps_add_section(sc, ps_ltrim(p + 1));
                    if (current >= 0) ps_rtrim(sc->sections[current].name);
                }
            }
            else if (current >= 0 && (eq = ps_findchar(p, '=')) != NULL)
            {
                *eq++ = '\0';
                ps_rtrim(p);
                eq = ps_ltrim(eq);
                (void)ps_set_entry(sc, sc->sections[current].name, p, eq);
            }
        }
        line = next;
    }

    free(data);
    return slot + 1;
}

void SCRIPT_Free(int32 handle)
{
    ps_script_t *sc = ps_handle(handle);
    if (sc) memset(sc, 0, sizeof(*sc));
}

int32 SCRIPT_NumberEntries(int32 handle, char *sectionname)
{
    ps_script_t *sc = ps_handle(handle);
    int si, i, n = 0;
    if (!sc) return 0;
    si = ps_find_section(sc, sectionname);
    if (si < 0) return 0;
    for (i = 0; i < sc->entry_count; ++i)
        if (sc->entries[i].section == si) ++n;
    return n;
}

char *SCRIPT_Entry(int32 handle, char *sectionname, int32 which)
{
    ps_script_t *sc = ps_handle(handle);
    int si, i;
    if (!sc || which < 0) return NULL;
    si = ps_find_section(sc, sectionname);
    if (si < 0) return NULL;
    for (i = 0; i < sc->entry_count; ++i)
        if (sc->entries[i].section == si && which-- == 0)
            return sc->entries[i].name;
    return NULL;
}

void SCRIPT_GetString(int32 handle, char *sectionname, char *entryname, char *dest)
{
    ps_script_t *sc = ps_handle(handle);
    char *v;
    if (!sc) return;
    v = ps_value(sc, sectionname, entryname);
    if (v) ps_unquote_one(v, dest);
}

void SCRIPT_GetDoubleString(int32 handle, char *sectionname, char *entryname,
                            char *dest1, char *dest2)
{
    ps_script_t *sc = ps_handle(handle);
    char *v, tmp[PS_VALUE_LEN];
    char *p, *comma = NULL;
    char quote = 0;
    if (!sc) return;
    v = ps_value(sc, sectionname, entryname);
    if (!v) return;
    ps_copy(tmp, sizeof(tmp), v);
    for (p = tmp; *p; ++p)
    {
        if (*p == '"' || *p == '\'')
        {
            if (!quote) quote = *p;
            else if (quote == *p) quote = 0;
        }
        else if (*p == ',' && !quote)
        {
            comma = p;
            break;
        }
    }
    if (comma)
    {
        *comma++ = '\0';
        ps_unquote_one(tmp, dest1);
        ps_unquote_one(comma, dest2);
    }
    else
        ps_unquote_one(tmp, dest1);
}

static boolean ps_parse_number(const char *text, int32 *number)
{
    const char *p = text;
    unsigned long value = 0;
    unsigned base = 10;
    int neg = 0, any = 0;
    while (*p && ps_space((unsigned char)*p)) ++p;
    if (*p == '+' || *p == '-')
    {
        neg = (*p == '-');
        ++p;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    while (*p)
    {
        unsigned digit;
        if (*p >= '0' && *p <= '9') digit = (unsigned)(*p - '0');
        else if (*p >= 'a' && *p <= 'f') digit = (unsigned)(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F') digit = (unsigned)(*p - 'A' + 10);
        else break;
        if (digit >= base) break;
        value = value * base + digit;
        any = 1;
        ++p;
    }
    if (!any) return false;
    *number = neg ? -(int32)value : (int32)value;
    return true;
}

boolean SCRIPT_GetNumber(int32 handle, char *sectionname, char *entryname,
                         int32 *number)
{
    ps_script_t *sc = ps_handle(handle);
    char *v;
    if (!sc) return false;
    v = ps_value(sc, sectionname, entryname);
    if (!v) return false;
    return ps_parse_number(v, number);
}

void SCRIPT_PutString(int32 handle, char *sectionname, char *entryname, char *string)
{
    ps_script_t *sc = ps_handle(handle);
    char value[PS_VALUE_LEN];
    if (!sc) return;
    sprintf(value, "\"%s\"", string ? string : "");
    (void)ps_set_entry(sc, sectionname, entryname, value);
}

void SCRIPT_PutNumber(int32 handle, char *sectionname, char *entryname,
                      int32 number, boolean hexadecimal, boolean defaultvalue)
{
    ps_script_t *sc = ps_handle(handle);
    char value[48];
    (void)defaultvalue;
    if (!sc) return;
    if (hexadecimal) sprintf(value, "0x%lx", (unsigned long)number);
    else sprintf(value, "%ld", (long)number);
    (void)ps_set_entry(sc, sectionname, entryname, value);
}

void SCRIPT_Save(int32 handle, char *filename)
{
    ps_script_t *sc = ps_handle(handle);
    FILE *f;
    int si, i;
    if (!sc) return;
    f = fopen(filename, "wt");
    if (!f) return;
    for (si = 0; si < sc->section_count; ++si)
    {
        fprintf(f, "[%s]\n", sc->sections[si].name);
        for (i = 0; i < sc->entry_count; ++i)
            if (sc->entries[i].section == si)
                fprintf(f, "%s = %s\n", sc->entries[i].name, sc->entries[i].value);
        fputc('\n', f);
    }
    fclose(f);
}
