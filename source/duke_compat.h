#ifndef DUKE_COMPAT_H
#define DUKE_COMPAT_H

/* Watcom/DOS compatibility needed by the original Duke 3D sources. */
#ifndef S_IREAD
#define S_IREAD 0
#endif
#ifndef S_IRUSR
#define S_IRUSR 0
#define S_IWUSR 0
#define S_IRGRP 0
#define S_IWGRP 0
#endif

#ifndef _A_NORMAL
#define _A_NORMAL 0x00
#endif
#ifndef _A_SUBDIR
#define _A_SUBDIR 0x10
#endif

struct find_t
{
    unsigned attrib;
    char name[260];
};

int _dos_findfirst(const char *pattern, unsigned attrib, struct find_t *info);
int _dos_findnext(struct find_t *info);

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif
