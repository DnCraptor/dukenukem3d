/* Portable C replacements for the non-self-modifying parts of SRC/A.ASM.
 * The original routines patch immediate operands in their own x86 code;
 * here those values are kept as ordinary module state. */
#include <stdint.h>
#include <stddef.h>

static long a_vline_bpl = 320;
static unsigned a_vline_shift;
static unsigned a_mvline_shift;
static unsigned a_tvline_shift;
static unsigned char *a_palookup;
static unsigned char *a_transluc;
static int a_trans_reverse;
static long a_slab_bpl;
static unsigned char *a_slab_pal;

static unsigned char *ptr8(long p)
{
    return (unsigned char *)(uintptr_t)(uint32_t)p;
}

long setvlinebpl(long bpl)
{
    a_vline_bpl = bpl;
    return bpl;
}

long setpalookupaddress(char *p)
{
    a_palookup = (unsigned char *)p;
    return (long)(uintptr_t)p;
}

long setupvlineasm(long shift)
{
    a_vline_shift = (unsigned)shift;
    return shift;
}

long setupmvlineasm(long shift)
{
    a_mvline_shift = (unsigned)shift;
    return shift;
}

long setuptvlineasm(long shift)
{
    a_tvline_shift = (unsigned)shift;
    return shift;
}

long fixtransluscence(long p)
{
    a_transluc = ptr8(p);
    return p;
}

long settransnormal(void)
{
    a_trans_reverse = 0;
    return 0;
}

long settransreverse(void)
{
    a_trans_reverse = 1;
    return 0;
}

long vlineasm1(long vinc, long pal, long cnt, long vplc, long buf, long dest)
{
    unsigned char *palette = ptr8(pal);
    unsigned char *src = ptr8(buf);
    unsigned char *dst = ptr8(dest);
    int32_t v = (int32_t)vplc;
    long n = cnt + 1;

    while (n-- > 0)
    {
        uint32_t idx = ((uint32_t)v) >> a_vline_shift;
        dst[0] = palette[src[idx]];
        v = (int32_t)((uint32_t)v + (uint32_t)vinc);
        dst += a_vline_bpl;
    }
    return (long)v;
}

long prevlineasm1(long vinc, long pal, long cnt, long vplc, long buf, long dest)
{
    if (cnt != 0)
        return vlineasm1(vinc,pal,cnt,vplc,buf,dest);

    vplc = (long)((uint32_t)vplc + (uint32_t)vinc);
    ptr8(dest)[0] = ptr8(pal)[ptr8(buf)[((uint32_t)vplc) >> a_vline_shift]];
    return vplc;
}

long mvlineasm1(long vinc, long pal, long cnt, long vplc, long buf, long dest)
{
    unsigned char *palette = ptr8(pal);
    unsigned char *src = ptr8(buf);
    unsigned char *dst = ptr8(dest);
    int32_t v = (int32_t)vplc;
    long n = cnt + 1;

    while (n-- > 0)
    {
        unsigned char texel = src[((uint32_t)v) >> a_mvline_shift];
        if (texel != 255)
            dst[0] = palette[texel];
        v = (int32_t)((uint32_t)v + (uint32_t)vinc);
        dst += a_vline_bpl;
    }
    return (long)v;
}

long tvlineasm1(long vinc, long pal, long cnt, long vplc, long buf, long dest)
{
    unsigned char *palette = ptr8(pal);
    unsigned char *src = ptr8(buf);
    unsigned char *dst = ptr8(dest);
    int32_t v = (int32_t)vplc;
    long n = cnt + 1;

    while (n-- > 0)
    {
        unsigned char texel = src[((uint32_t)v) >> a_tvline_shift];
        if (texel != 255)
        {
            unsigned char fg = palette[texel];
            unsigned index = a_trans_reverse ? (((unsigned)fg << 8) | dst[0])
                                             : (((unsigned)dst[0] << 8) | fg);
            dst[0] = a_transluc[index];
        }
        v = (int32_t)((uint32_t)v + (uint32_t)vinc);
        dst += a_vline_bpl;
    }
    return (long)v;
}

long setupdrawslab(long bpl, long pal)
{
    a_slab_bpl = bpl;
    a_slab_pal = ptr8(pal);
    return bpl;
}

long drawslab(long dx, long v, long dy, long vinc, long data, long dest)
{
    const unsigned char *src = ptr8(data);
    unsigned char *dst = ptr8(dest);
    int32_t vv = (int32_t)v;
    long y;

    for (y = 0; y < dy; ++y)
    {
        unsigned char c = a_slab_pal[src[((uint32_t)vv) >> 16]];
        long x;
        for (x = 0; x < dx; ++x) dst[x] = c;
        vv = (int32_t)((uint32_t)vv + (uint32_t)vinc);
        dst += a_slab_bpl;
    }
    return (long)vv;
}
