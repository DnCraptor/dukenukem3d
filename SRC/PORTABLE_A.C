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

    /* A.ASM samples with the old VPLC, then returns VPLC+VINC. */
    ptr8(dest)[0] = ptr8(pal)[ptr8(buf)[((uint32_t)vplc) >> a_vline_shift]];
    return (long)(int32_t)((uint32_t)vplc + (uint32_t)vinc);
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

/* Four adjacent vertical columns.  A.ASM packs the four texture accumulators
 * into registers for speed; the visible state is simply vplce[0..3]. */
extern long vplce[4], vince[4], palookupoffse[4], bufplce[4];

long vlineasm4(long cnt, long dest)
{
    unsigned char *dst = ptr8(dest);
    long n;
    int x;

    for (n = 0; n < cnt; ++n)
    {
        for (x = 0; x < 4; ++x)
        {
            unsigned char *src = ptr8(bufplce[x]);
            unsigned char *pal = ptr8(palookupoffse[x]);
            dst[x] = pal[src[((uint32_t)vplce[x]) >> a_vline_shift]];
            vplce[x] = (long)((uint32_t)vplce[x] + (uint32_t)vince[x]);
        }
        dst += a_vline_bpl;
    }
    return 0;
}

long mvlineasm4(long cnt, long dest)
{
    unsigned char *dst = ptr8(dest);
    long n;
    int x;

    for (n = 0; n < cnt; ++n)
    {
        for (x = 0; x < 4; ++x)
        {
            unsigned char *src = ptr8(bufplce[x]);
            unsigned char texel = src[((uint32_t)vplce[x]) >> a_mvline_shift];
            if (texel != 255)
                dst[x] = ptr8(palookupoffse[x])[texel];
            vplce[x] = (long)((uint32_t)vplce[x] + (uint32_t)vince[x]);
        }
        dst += a_vline_bpl;
    }
    return 0;
}

static unsigned char *a_sprite_pal;
static uint32_t a_sprite_xfrac_inc;
static int32_t a_sprite_base_inc;
static int32_t a_sprite_ywrap_inc;
static uint32_t a_sprite_yfrac_inc;

static void setup_sprite_common(long pal, long baseinc, long yfracinc,
                                long ywrapinc, long xfracinc)
{
    a_sprite_pal = ptr8(pal);
    a_sprite_xfrac_inc = (uint32_t)xfracinc << 16;
    a_sprite_base_inc = (int32_t)baseinc + ((int32_t)xfracinc >> 16);
    a_sprite_ywrap_inc = (int32_t)ywrapinc;
    a_sprite_yfrac_inc = (uint32_t)yfracinc;
}

void setupspritevline(long pal, long baseinc, long yfracinc,
                      long ywrapinc, long xfracinc, long unused)
{
    (void)unused;
    setup_sprite_common(pal,baseinc,yfracinc,ywrapinc,xfracinc);
}

void msetupspritevline(long pal, long baseinc, long yfracinc,
                       long ywrapinc, long xfracinc, long unused)
{
    (void)unused;
    setup_sprite_common(pal,baseinc,yfracinc,ywrapinc,xfracinc);
}

void tsetupspritevline(long pal, long baseinc, long yfracinc,
                       long ywrapinc, long xfracinc, long unused)
{
    (void)unused;
    setup_sprite_common(pal,baseinc,yfracinc,ywrapinc,xfracinc);
}

static void sprite_vline_common(long xfrac, long cnt, long yfrac,
                                long srcp, long dest, int masked, int translucent)
{
    uint32_t xf = (uint32_t)xfrac;
    uint32_t yf = (uint32_t)yfrac;
    unsigned char *src = ptr8(srcp);
    unsigned char *dst = ptr8(dest);
    long n;

    /* A.ASM receives (y2-y1+1) and decrements before the first store. */
    for (n = 1; n < cnt; ++n)
    {
        uint32_t old_y = yf;
        uint32_t old_x = xf;
        int32_t advance;
        unsigned char texel;

        yf += a_sprite_yfrac_inc;
        xf += a_sprite_xfrac_inc;
        advance = a_sprite_base_inc;
        if (yf < old_y) advance += a_sprite_ywrap_inc;
        if (xf < old_x) advance += 1;
        src += advance;

        texel = *src;
        if (!masked || texel != 255)
        {
            unsigned char fg = a_sprite_pal[texel];
            if (translucent)
            {
                unsigned index = a_trans_reverse ? (((unsigned)fg << 8) | dst[0])
                                                 : (((unsigned)dst[0] << 8) | fg);
                dst[0] = a_transluc[index];
            }
            else
                dst[0] = fg;
        }
        dst += a_vline_bpl;
    }
}

void spritevline(long unused, long xfrac, long cnt, long yfrac, long src, long dest)
{
    (void)unused;
    sprite_vline_common(xfrac,cnt,yfrac,src,dest,0,0);
}

void mspritevline(long unused, long xfrac, long cnt, long yfrac, long src, long dest)
{
    (void)unused;
    sprite_vline_common(xfrac,cnt,yfrac,src,dest,1,0);
}

void tspritevline(long unused, long xfrac, long cnt, long yfrac, long src, long dest)
{
    (void)unused;
    sprite_vline_common(xfrac,cnt,yfrac,src,dest,1,1);
}

/* Two adjacent translucent columns.  In A.ASM _asm1 is used as vinc2 on
 * entry and receives vplc1 on return; _asm2 is the inclusive address of the
 * final right-hand pixel and receives vplc2 on return. */
extern long asm1, asm2;

static unsigned a_tvline2_shift;
static unsigned char *a_tvline2_pal[2];

long setuptvlineasm2(long shift, long pal1, long pal2)
{
    a_tvline2_shift = (unsigned)shift;
    a_tvline2_pal[0] = ptr8(pal1);
    a_tvline2_pal[1] = ptr8(pal2);
    return shift;
}

static unsigned char translucent_pixel(unsigned char background,
                                       unsigned char foreground)
{
    unsigned index;

    if (a_trans_reverse)
        index = ((unsigned)foreground << 8) | background;
    else
        index = ((unsigned)background << 8) | foreground;
    return a_transluc[index];
}

long tvlineasm2(long vplc2, long vinc1, long buf1, long buf2,
                long vplc1, long dest)
{
    unsigned char *src1 = ptr8(buf1);
    unsigned char *src2 = ptr8(buf2);
    unsigned char *dst = ptr8(dest);
    unsigned char *end = ptr8(asm2);
    int32_t v1 = (int32_t)vplc1;
    int32_t v2 = (int32_t)vplc2;
    int32_t inc1 = (int32_t)vinc1;
    int32_t inc2 = (int32_t)asm1;

    while ((uintptr_t)(dst + 1) <= (uintptr_t)end)
    {
        unsigned char t1 = src1[((uint32_t)v1) >> a_tvline2_shift];
        unsigned char t2 = src2[((uint32_t)v2) >> a_tvline2_shift];

        v1 = (int32_t)((uint32_t)v1 + (uint32_t)inc1);
        v2 = (int32_t)((uint32_t)v2 + (uint32_t)inc2);

        if (t1 != 255)
            dst[0] = translucent_pixel(dst[0],a_tvline2_pal[0][t1]);
        if (t2 != 255)
            dst[1] = translucent_pixel(dst[1],a_tvline2_pal[1][t2]);

        dst += a_vline_bpl;
    }

    asm1 = (long)v1;
    asm2 = (long)v2;
    return 0;
}

/* Masked/translucent horizontal spans.  A.ASM patches the two texture
 * dimensions into SHR/SHLD immediates; keep the dimensions as state instead. */
static unsigned a_hline_xbits;
static unsigned a_hline_ybits;

long msethlineshift(long xbits, long ybits)
{
    a_hline_xbits = (unsigned)xbits;
    a_hline_ybits = (unsigned)ybits;
    return xbits;
}

long tsethlineshift(long xbits, long ybits)
{
    a_hline_xbits = (unsigned)xbits;
    a_hline_ybits = (unsigned)ybits;
    return xbits;
}

static unsigned char *a_hline_texture;

long sethlinesizes(long xbits, long ybits, long texture)
{
    a_hline_xbits = (unsigned)xbits;
    a_hline_ybits = (unsigned)ybits;
    a_hline_texture = ptr8(texture);
    return xbits;
}

/* In the selected (pro*) A.ASM path setuphlineasm4 is patched to RET.
 * asm1/asm2 are read directly by hlineasm4. */
long setuphlineasm4(long xinc, long yinc)
{
    (void)yinc;
    return xinc;
}

static uint32_t hline_texel_index(uint32_t bx, uint32_t by)
{
    uint32_t xpart;
    uint32_t ypart;

    if (a_hline_xbits == 0)
        xpart = 0;
    else
        xpart = bx >> (32u - a_hline_xbits);

    if (a_hline_ybits == 0)
        ypart = 0;
    else
        ypart = by >> (32u - a_hline_ybits);

    return (xpart << a_hline_ybits) | ypart;
}

extern long asm3;

long hlineasm4(long cnt, long unused, long paloffs, long by, long bx, long dest)
{
    unsigned char *dst = ptr8(dest);
    unsigned char *shade = a_palookup + (uint32_t)paloffs;
    uint32_t x = (uint32_t)bx;
    uint32_t y = (uint32_t)by;
    uint32_t xinc = (uint32_t)asm1;
    uint32_t yinc = (uint32_t)asm2;
    long n = cnt + 1;

    (void)unused;

    while (n-- > 0)
    {
        unsigned char texel = a_hline_texture[hline_texel_index(x,y)];
        *dst-- = shade[texel];
        x -= xinc;
        y -= yinc;
    }

    return 0;
}

static long masked_hline_common(long texture, long bx, long cnt, long by,
                                long dest, int translucent)
{
    const unsigned char *tex = ptr8(texture);
    const unsigned char *shade = ptr8(asm3);
    unsigned char *dst = ptr8(dest);
    uint32_t x = (uint32_t)bx;
    uint32_t y = (uint32_t)by;
    uint32_t xinc = (uint32_t)asm1;
    uint32_t yinc = (uint32_t)asm2;
    unsigned long pixels = ((uint32_t)cnt >> 16) + 1u;

    while (pixels-- != 0)
    {
        unsigned char texel = tex[hline_texel_index(x,y)];

        if (texel != 255)
        {
            unsigned char fg = shade[texel];
            if (translucent)
                *dst = translucent_pixel(*dst,fg);
            else
                *dst = fg;
        }

        x += xinc;
        y += yinc;
        ++dst;
    }

    return 0;
}

long mhline(long texture, long bx, long cnt, long unused, long by, long dest)
{
    (void)unused;
    return masked_hline_common(texture,bx,cnt,by,dest,0);
}

long thline(long texture, long bx, long cnt, long unused, long by, long dest)
{
    (void)unused;
    return masked_hline_common(texture,bx,cnt,by,dest,1);
}

/* Rotated/non-power-of-two horizontal spans used by rotatesprite.  The x86
 * routines walk the destination from right to left and maintain the texture
 * address as a split low/high fixed-point accumulator. */
static uint32_t a_rh_xlo_inc;
static uint32_t a_rh_ylo_inc;
static uint32_t a_rh_hi_inc;
static uint32_t a_rh_ysize;
static unsigned char *a_rh_pal;

static uint32_t a_qrh_ylo_inc;
static uint32_t a_qrh_hi_inc;
static unsigned char *a_qrh_pal;

long setuprhlineasm4(long xloinc, long yloinc, long hiinc,
                     long pal, long ysize, long unused)
{
    (void)unused;
    a_rh_xlo_inc = (uint32_t)xloinc;
    a_rh_ylo_inc = (uint32_t)yloinc;
    a_rh_hi_inc = (uint32_t)hiinc;
    a_rh_ysize = (uint32_t)ysize;
    a_rh_pal = ptr8(pal);
    return xloinc;
}

long setuprmhlineasm4(long xloinc, long yloinc, long hiinc,
                      long pal, long ysize, long unused)
{
    return setuprhlineasm4(xloinc,yloinc,hiinc,pal,ysize,unused);
}

static long rotated_hline_common(long cnt, long texture, long xlo, long ylo,
                                 long dest, int masked)
{
    uint32_t texaddr = (uint32_t)texture;
    uint32_t x = (uint32_t)xlo;
    uint32_t y = (uint32_t)ylo;
    unsigned char *dst = ptr8(dest);
    long n;

    for (n = 0; n < cnt; ++n)
    {
        unsigned char texel = *ptr8((long)texaddr);
        uint32_t oldx = x;
        uint32_t oldy = y;
        uint32_t borrowx;
        uint32_t borrowy;

        --dst;
        if (!masked || texel != 255)
            *dst = a_rh_pal[texel];

        x -= a_rh_xlo_inc;
        borrowx = (oldx < a_rh_xlo_inc);
        y -= a_rh_ylo_inc;
        borrowy = (oldy < a_rh_ylo_inc);

        texaddr -= a_rh_hi_inc + borrowy;
        if (borrowx)
            texaddr -= a_rh_ysize;
    }

    return 0;
}

long rhlineasm4(long cnt, long texture, long unused, long xlo,
                long ylo, long dest)
{
    (void)unused;
    return rotated_hline_common(cnt,texture,xlo,ylo,dest,0);
}

long rmhlineasm4(long cnt, long texture, long unused, long xlo,
                 long ylo, long dest)
{
    (void)unused;
    return rotated_hline_common(cnt,texture,xlo,ylo,dest,1);
}

long setupqrhlineasm4(long unused, long yloinc, long hiinc,
                      long pal, long unused2, long unused3)
{
    (void)unused;
    (void)unused2;
    (void)unused3;
    a_qrh_ylo_inc = (uint32_t)yloinc;
    a_qrh_hi_inc = (uint32_t)hiinc;
    a_qrh_pal = ptr8(pal);
    return yloinc;
}

long qrhlineasm4(long cnt, long texture, long unused, long unused2,
                 long ylo, long dest)
{
    uint32_t texaddr = (uint32_t)texture;
    uint32_t y = (uint32_t)ylo;
    unsigned char *dst = ptr8(dest);
    long n;

    (void)unused;
    (void)unused2;

    for (n = 0; n < cnt; ++n)
    {
        unsigned char texel = *ptr8((long)texaddr);
        uint32_t oldy = y;

        --dst;
        *dst = a_qrh_pal[texel];

        y -= a_qrh_ylo_inc;
        texaddr -= a_qrh_hi_inc + (oldy < a_qrh_ylo_inc);
    }

    return 0;
}

/* Perspective-correct sloped floor/ceiling mapper.  A.ASM keeps the
 * denominator in x87 extended precision, but rounds it to single precision
 * when deriving the reciprocal for each group of up to 8 pixels. */
extern long asm3;
extern long globalx3, globaly3;
extern long reciptable[2048];

static unsigned a_slope_xbits;
static unsigned a_slope_ybits;
static unsigned char *a_slope_texture;
static long a_slope_pinc;
static float a_slope_delta;

static uint32_t slope_recip_from_float(float value)
{
    union { float f; uint32_t u; } cv;
    uint32_t eax, ecx, signmask;
    unsigned shift;
    unsigned index;

    cv.f = value;
    eax = cv.u;
    signmask = (eax & 0x80000000u) ? 0xffffffffu : 0u;
    eax <<= 1;
    ecx = eax >> 24;
    index = ((eax & 0x00ffe000u) >> 11) >> 2;
    shift = ((unsigned char)((unsigned char)ecx - 2u)) & 31u;
    eax = (uint32_t)reciptable[index];
    eax >>= shift;
    return eax ^ signmask;
}

long setupslopevlin(long bits, long texture, long pinc)
{
    union { float f; uint32_t u; } cv;

    a_slope_xbits = (unsigned char)bits;
    a_slope_ybits = (unsigned char)((uint32_t)bits >> 8);
    a_slope_texture = ptr8(texture);
    a_slope_pinc = pinc;

    a_slope_delta = (float)(int32_t)asm1;
    cv.f = a_slope_delta;
    asm2 = (long)(int32_t)cv.u;
    return bits;
}

long slopevlin(long dest, long recip, long palptr, long count,
               long xplc, long yplc)
{
    unsigned char *dst = ptr8(dest);
    long *pal = (long *)(uintptr_t)(uint32_t)palptr;
    uint32_t x = (uint32_t)xplc;
    uint32_t y = (uint32_t)yplc;
    uint32_t oldrecip = (uint32_t)recip;
    uint32_t scaledrecip = oldrecip << 3;
    uint32_t xmask;
    unsigned xshift = 32u - (a_slope_xbits + a_slope_ybits);
    unsigned yshift = 32u - a_slope_ybits;
    double denom = (double)(int32_t)asm3 + (double)a_slope_delta;
    long remaining = count;

    if (a_slope_xbits == 0)
        xmask = 0;
    else
        xmask = ((1u << a_slope_xbits) - 1u) << a_slope_ybits;

    x += (uint32_t)((uint64_t)(uint32_t)globalx3 * scaledrecip);
    y += (uint32_t)((uint64_t)(uint32_t)globaly3 * scaledrecip);
    asm1 = (long)(int32_t)oldrecip;

    while (remaining > 0)
    {
        uint32_t newrecip = slope_recip_from_float((float)denom);
        uint32_t delta = newrecip - oldrecip;
        uint32_t xinc = (uint32_t)((uint64_t)(uint32_t)globalx3 * delta);
        uint32_t yinc = (uint32_t)((uint64_t)(uint32_t)globaly3 * delta);
        long n = (remaining < 8) ? remaining : 8;

        asm1 = (long)(int32_t)newrecip;
        denom += (double)a_slope_delta;

        while (n-- > 0)
        {
            uint32_t texindex;
            unsigned char texel;
            unsigned char *palette;

            texindex = ((x >> xshift) & xmask) | (y >> yshift);
            texel = a_slope_texture[texindex];
            palette = ptr8(*pal--);
            *dst = palette[texel];

            x += xinc;
            y += yinc;
            dst += a_slope_pinc;
        }

        oldrecip = newrecip;
        remaining -= 8;
    }

    return 0;
}
