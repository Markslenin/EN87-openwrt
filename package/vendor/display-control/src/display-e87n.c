/*
 * E87N optimized V4 renderer
 * - PingFang/Noto labels with Open Sans Condensed Light display numerals
 * - startup background fades from pure black into deep blue
 * - true circular ripple in pixel space
 * - central square edges morph into dashboard structure
 * - staged dashboard reveal and RGB565-safe palette
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <net/if.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LCD_W 428
#define LCD_H 142
#define TARGET_FPS 30
#define FRAME_NS 33333333L
#define TILE_PITCH 8
#define GRID_COLS 53
#define GRID_ROWS 17
#define TILE_COUNT (GRID_COLS * GRID_ROWS)
#define GRAPH_COLS 36
#define MAX_PARTICLES 360
#define MAX_TARGETS 4096
#define BOOT_DEFAULT_TIMEOUT 25.0
#define TRANSITION_DURATION 1.58
#define FOCUS_DURATION 0.22

#define FONT_PF_REG   "/usr/share/display-e87n/PingFangSC-Regular.ttf"
#define FONT_PF_SEMI  "/usr/share/display-e87n/PingFangSC-Semibold.ttf"
#define FONT_PF_TTC   "/usr/share/display-e87n/PingFang.ttc"
#define FONT_NOTO_REG "/usr/share/display-e87n/NotoSansCJKsc-Regular.otf"
#define FONT_NOTO_BOLD "/usr/share/display-e87n/NotoSansCJKsc-Bold.otf"
#define FONT_NUM_OPEN   "/usr/share/display-e87n/OpenSans-CondLight.ttf"
#define FONT_NUM_SYSTEM "/usr/share/fonts/truetype/open-sans/OpenSans-CondLight.ttf"
#define FONT_NUM_NOTO   "/usr/share/display-e87n/NotoSansDisplay-ExtraCondensedLight.ttf"
#define FONT_FALLBACK "/usr/share/display-e87n/Oswald.ttf"
#define HEARTBEAT_PATH "/tmp/display-e87n.heartbeat"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_ready = 0;

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}
static inline float smoothstep01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}
static inline float smootherstep01(float x) {
    x = clamp01(x);
    return x*x*x*(x*(x*6.0f - 15.0f) + 10.0f);
}
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline int lerpi(int a, int b, float t) { return (int)lroundf(lerpf((float)a, (float)b, t)); }

static double mono_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) g_running = 0;
    else if (sig == SIGUSR1) g_ready = 1;
}

static uint16_t rgb565(int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3));
}
static void unpack565(uint16_t c, int *r, int *g, int *b) {
    *r = ((c >> 11) & 0x1f) * 255 / 31;
    *g = ((c >> 5) & 0x3f) * 255 / 63;
    *b = (c & 0x1f) * 255 / 31;
}
static uint16_t mix565(uint16_t a, uint16_t b, float t) {
    int ar, ag, ab, br, bg, bb;
    t = clamp01(t);
    unpack565(a, &ar, &ag, &ab);
    unpack565(b, &br, &bg, &bb);
    return rgb565(lerpi(ar, br, t), lerpi(ag, bg, t), lerpi(ab, bb, t));
}
static uint16_t blend565(uint16_t dst, uint16_t src, uint8_t alpha) {
    return mix565(dst, src, (float)alpha / 255.0f);
}
static uint16_t scale565(uint16_t c, float s) {
    int r, g, b;
    unpack565(c, &r, &g, &b);
    return rgb565((int)(r*s), (int)(g*s), (int)(b*s));
}
static int luma565(uint16_t c) {
    int r,g,b;
    unpack565(c,&r,&g,&b);
    return (r*54 + g*183 + b*19) >> 8;
}

typedef struct {
    int fd;
    uint8_t *map;
    size_t map_len;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
} FbCtx;

typedef struct {
    uint16_t *pix;
    int w, h;
} Canvas;

static int fb_open(FbCtx *fb, const char *path) {
    memset(fb, 0, sizeof(*fb));
    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0) { perror("open framebuffer"); return -1; }
    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo) < 0 ||
        ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) < 0) {
        perror("framebuffer ioctl"); close(fb->fd); return -1;
    }
    if (fb->vinfo.bits_per_pixel != 16) {
        fprintf(stderr, "Only 16-bpp framebuffer is supported, got %u\n", fb->vinfo.bits_per_pixel);
        close(fb->fd); return -1;
    }
    if ((int)fb->vinfo.xres < LCD_W || (int)fb->vinfo.yres < LCD_H) {
        fprintf(stderr, "Framebuffer too small: %ux%u\n", fb->vinfo.xres, fb->vinfo.yres);
        close(fb->fd); return -1;
    }
    fb->map_len = fb->finfo.smem_len;
    fb->map = mmap(NULL, fb->map_len, PROT_READ|PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->map == MAP_FAILED) { perror("mmap framebuffer"); close(fb->fd); return -1; }
    return 0;
}
static void fb_close(FbCtx *fb) {
    if (fb->map && fb->map != MAP_FAILED) munmap(fb->map, fb->map_len);
    if (fb->fd >= 0) close(fb->fd);
}
static void fb_present(const FbCtx *fb, const Canvas *c) {
    int xoff = (int)fb->vinfo.xoffset;
    int yoff = (int)fb->vinfo.yoffset;
    for (int y=0; y<c->h; ++y) {
        uint8_t *dst = fb->map + (size_t)(y+yoff)*fb->finfo.line_length + (size_t)xoff*2;
        memcpy(dst, &c->pix[y*c->w], (size_t)c->w*2);
    }
}

static void put_px(Canvas *c, int x, int y, uint16_t color) {
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h) return;
    c->pix[y*c->w+x] = color;
}
static void blend_px(Canvas *c, int x, int y, uint16_t color, uint8_t alpha) {
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h) return;
    uint16_t *p = &c->pix[y*c->w+x];
    *p = blend565(*p, color, alpha);
}
static void fill_rect(Canvas *c, int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x+w > c->w ? c->w : x+w, y1 = y+h > c->h ? c->h : y+h;
    for (int yy=y0; yy<y1; ++yy)
        for (int xx=x0; xx<x1; ++xx) c->pix[yy*c->w+xx] = color;
}
static void fill_rect_alpha(Canvas *c, int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x+w > c->w ? c->w : x+w, y1 = y+h > c->h ? c->h : y+h;
    for (int yy=y0; yy<y1; ++yy)
        for (int xx=x0; xx<x1; ++xx) blend_px(c,xx,yy,color,alpha);
}
static void hline(Canvas *c, int x0, int x1, int y, uint16_t color) {
    if (y<0 || y>=c->h) return;
    if (x0>x1) { int t=x0; x0=x1; x1=t; }
    if (x0<0) x0=0; if (x1>=c->w) x1=c->w-1;
    for (int x=x0; x<=x1; ++x) c->pix[y*c->w+x]=color;
}
static void vline(Canvas *c, int x, int y0, int y1, uint16_t color) {
    if (x<0 || x>=c->w) return;
    if (y0>y1) { int t=y0; y0=y1; y1=t; }
    if (y0<0) y0=0; if (y1>=c->h) y1=c->h-1;
    for (int y=y0; y<=y1; ++y) c->pix[y*c->w+x]=color;
}
static void draw_line(Canvas *c, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx=abs(x1-x0), sx=x0<x1?1:-1;
    int dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int err=dx+dy;
    for (;;) {
        put_px(c,x0,y0,color);
        if (x0==x1 && y0==y1) break;
        int e2=2*err;
        if (e2>=dy) { err+=dy; x0+=sx; }
        if (e2<=dx) { err+=dx; y0+=sy; }
    }
}
static void rect_outline(Canvas *c, int x, int y, int w, int h, int stroke, uint16_t color) {
    for (int i=0;i<stroke;++i) {
        hline(c,x+i,x+w-1-i,y+i,color);
        hline(c,x+i,x+w-1-i,y+h-1-i,color);
        vline(c,x+i,y+i,y+h-1-i,color);
        vline(c,x+w-1-i,y+i,y+h-1-i,color);
    }
}
static void rounded_rect(Canvas *c, int x, int y, int w, int h, int r, uint16_t color) {
    fill_rect(c,x+r,y,w-2*r,h,color);
    fill_rect(c,x,y+r,r,h-2*r,color);
    fill_rect(c,x+w-r,y+r,r,h-2*r,color);
    for (int yy=0; yy<r; ++yy) {
        for (int xx=0; xx<r; ++xx) {
            int dx=r-1-xx, dy=r-1-yy;
            if (dx*dx+dy*dy <= r*r) {
                put_px(c,x+xx,y+yy,color);
                put_px(c,x+w-1-xx,y+yy,color);
                put_px(c,x+xx,y+h-1-yy,color);
                put_px(c,x+w-1-xx,y+h-1-yy,color);
            }
        }
    }
}

typedef struct {
    FT_Library lib;
    FT_Face regular;      /* PingFang / Noto: labels and body text */
    FT_Face semibold;     /* PingFang Semibold: headings */
    FT_Face number;       /* Condensed light display numerals */
} Fonts;

static int face_try(FT_Library lib, const char *path, long index, FT_Face *out) {
    if (access(path, R_OK) != 0) return -1;
    return FT_New_Face(lib, path, index, out);
}
static int fonts_init(Fonts *f) {
    memset(f,0,sizeof(*f));
    if (FT_Init_FreeType(&f->lib)) return -1;
    if (face_try(f->lib,FONT_PF_REG,0,&f->regular) &&
        face_try(f->lib,FONT_PF_TTC,0,&f->regular) &&
        face_try(f->lib,FONT_NOTO_REG,0,&f->regular) &&
        face_try(f->lib,FONT_FALLBACK,0,&f->regular)) {
        fprintf(stderr,"No usable regular font found.\n"); return -1;
    }
    if (face_try(f->lib,FONT_PF_SEMI,0,&f->semibold) &&
        face_try(f->lib,FONT_PF_TTC,0,&f->semibold) &&
        face_try(f->lib,FONT_NOTO_BOLD,0,&f->semibold) &&
        face_try(f->lib,FONT_FALLBACK,0,&f->semibold)) {
        f->semibold = f->regular;
    }
    if (face_try(f->lib,FONT_NUM_OPEN,0,&f->number) &&
        face_try(f->lib,FONT_NUM_SYSTEM,0,&f->number) &&
        face_try(f->lib,FONT_NUM_NOTO,0,&f->number) &&
        face_try(f->lib,FONT_FALLBACK,0,&f->number)) {
        f->number = f->regular;
    }
    return 0;
}
static void fonts_close(Fonts *f) {
    if (f->number && f->number != f->regular && f->number != f->semibold) FT_Done_Face(f->number);
    if (f->semibold && f->semibold != f->regular) FT_Done_Face(f->semibold);
    if (f->regular) FT_Done_Face(f->regular);
    if (f->lib) FT_Done_FreeType(f->lib);
}

static uint32_t utf8_next(const char **s) {
    const unsigned char *p=(const unsigned char*)*s;
    uint32_t cp;
    if (*p<0x80) { cp=*p++; }
    else if ((*p&0xe0)==0xc0) { cp=(*p++&0x1f)<<6; cp|=*p++&0x3f; }
    else if ((*p&0xf0)==0xe0) { cp=(*p++&0x0f)<<12; cp|=(*p++&0x3f)<<6; cp|=*p++&0x3f; }
    else if ((*p&0xf8)==0xf0) { cp=(*p++&7)<<18; cp|=(*p++&0x3f)<<12; cp|=(*p++&0x3f)<<6; cp|=*p++&0x3f; }
    else { cp='?'; p++; }
    *s=(const char*)p; return cp;
}
static int text_measure(FT_Face face, int px, const char *text, float sx, int tracking) {
    FT_Set_Pixel_Sizes(face,0,(FT_UInt)px);
    FT_Matrix matrix={ (FT_Fixed)lroundf(sx*65536.0f),0,0,65536 };
    FT_Set_Transform(face,&matrix,NULL);
    int width=0, count=0;
    const char *p=text;
    while (*p) {
        uint32_t cp=utf8_next(&p);
        if (!FT_Load_Char(face,cp,FT_LOAD_DEFAULT)) width += (int)(face->glyph->advance.x>>6);
        count++;
    }
    FT_Set_Transform(face,NULL,NULL);
    if (count>1) width += tracking*(count-1);
    return width;
}
static void draw_text_ex(Canvas *c, FT_Face face, int px, int x, int y_top,
                         const char *text, uint16_t color, float sx, int tracking, uint8_t alpha_mul) {
    FT_Set_Pixel_Sizes(face,0,(FT_UInt)px);
    FT_Matrix matrix={ (FT_Fixed)lroundf(sx*65536.0f),0,0,65536 };
    FT_Set_Transform(face,&matrix,NULL);
    int asc=(int)(face->size->metrics.ascender>>6);
    int baseline=y_top+asc;
    int pen=x;
    const char *p=text;
    while (*p) {
        uint32_t cp=utf8_next(&p);
        if (FT_Load_Char(face,cp,FT_LOAD_RENDER|FT_LOAD_TARGET_LIGHT)) continue;
        FT_GlyphSlot g=face->glyph;
        int gx=pen+g->bitmap_left;
        int gy=baseline-g->bitmap_top;
        for (int by=0; by<(int)g->bitmap.rows; ++by) {
            const uint8_t *row=g->bitmap.buffer + by*g->bitmap.pitch;
            for (int bx=0; bx<(int)g->bitmap.width; ++bx) {
                uint8_t a=row[bx];
                a=(uint8_t)((unsigned)a*alpha_mul/255u);
                if (a) blend_px(c,gx+bx,gy+by,color,a);
            }
        }
        pen += (int)(g->advance.x>>6)+tracking;
    }
    FT_Set_Transform(face,NULL,NULL);
}
static void draw_text(Canvas *c, FT_Face face, int px, int x, int y, const char *s, uint16_t color) {
    draw_text_ex(c,face,px,x,y,s,color,1.0f,0,255);
}
static void draw_text_glow(Canvas *c, FT_Face face, int px, int x, int y,
                           const char *s, uint16_t main, uint16_t glow) {
    draw_text_ex(c,face,px,x-1,y,s,glow,1.0f,0,55);
    draw_text_ex(c,face,px,x+1,y,s,glow,1.0f,0,55);
    draw_text_ex(c,face,px,x,y,s,main,1.0f,0,255);
}
static void draw_text_right(Canvas *c, FT_Face face, int px, int right, int y,
                            const char *s, uint16_t color) {
    int w=text_measure(face,px,s,1.0f,0);
    draw_text(c,face,px,right-w,y,s,color);
}
static void draw_text_center(Canvas *c, FT_Face face, int px, int x0, int x1, int y,
                             const char *s, uint16_t color) {
    int w=text_measure(face,px,s,1.0f,0);
    draw_text(c,face,px,(x0+x1-w)/2,y,s,color);
}
static void draw_text_fit_width(Canvas *c, FT_Face face, int px, int x, int y,
                                int target_w, const char *s, uint16_t main, uint16_t glow) {
    int natural=text_measure(face,px,s,1.0f,0);
    float sx = natural>0 ? (float)target_w/(float)natural : 1.0f;
    if (sx<0.90f) sx=0.90f;
    if (sx>1.12f) sx=1.12f;
    int actual=text_measure(face,px,s,sx,0);
    int xx=x+(target_w-actual)/2;
    draw_text_ex(c,face,px,xx-1,y,s,glow,sx,0,50);
    draw_text_ex(c,face,px,xx+1,y,s,glow,sx,0,50);
    draw_text_ex(c,face,px,xx,y,s,main,sx,0,255);
}

/* UI colors */
static uint16_t UI_BG_EDGE, UI_BG_CENTER, UI_PANEL, UI_PANEL_DARK, UI_TEXT_MAIN,
    UI_TEXT_BRIGHT, UI_TEXT_MID, UI_TEXT_DIM, UI_LINE, UI_GLOW,
    UI_GRAPH_TOP, UI_GRAPH_LOW, UI_SEC_BG, UI_SEC_TEXT;
static uint16_t BOOT_BG_EDGE, BOOT_BG_CENTER, BOOT_TILE_IDLE, BOOT_TILE_HIGH, BOOT_TILE_PEAK,
    BOOT_CORE_LOW, BOOT_CORE_PEAK;

static unsigned fixed_hash(unsigned x, unsigned y) {
    unsigned v=x*73856093u ^ y*19349663u;
    v ^= v>>13; v*=1274126177u; v ^= v>>16;
    return v;
}
static void render_dashboard_background(Canvas *c) {
    for (int y=0;y<c->h;++y) {
        for (int x=0;x<c->w;++x) {
            float nx=((float)x-LCD_W*0.5f)/(LCD_W*0.5f);
            float ny=((float)y-LCD_H*0.5f)/(LCD_H*0.5f);
            float radius=sqrtf(nx*nx*0.61f+ny*ny);
            float a=0.18f+0.58f*clamp01(1.0f-radius);
            uint16_t col=mix565(UI_BG_EDGE,UI_BG_CENTER,a);
            int n=(int)(fixed_hash((unsigned)x,(unsigned)y)&3u)-1;
            if (n>0) col=mix565(col,UI_TEXT_DIM,0.012f*(float)n);
            if (y&1) col=scale565(col,0.975f);
            c->pix[y*c->w+x]=col;
        }
    }
    fill_rect_alpha(c,0,0,139,LCD_H,UI_BG_EDGE,75);
    fill_rect_alpha(c,145,23,278,63,UI_PANEL_DARK,145);
    fill_rect_alpha(c,145,89,278,35,UI_PANEL,135);
}

typedef struct {
    int cpu, mem, clients;
    float temp;
    char time_s[8], sec_s[4], date_s[20], ip[32];
    char max_s[16], min_s[16];
    int graph[GRAPH_COLS];
    uint64_t last_net_bytes;
    double last_net_time;
    double next_graph_sample;
    unsigned long long prev_total, prev_idle;
    char iface[IFNAMSIZ];
    int iface_ready;
} Metrics;

static void write_heartbeat(void) {
    FILE *fp=fopen(HEARTBEAT_PATH,"w");
    if (fp) { fprintf(fp,"%llu\n",(unsigned long long)time(NULL)); fclose(fp); }
}

static int read_u64_file(const char *path, uint64_t *v) {
    FILE *fp=fopen(path,"r"); if(!fp) return -1;
    unsigned long long x=0; int ok=fscanf(fp,"%llu",&x)==1; fclose(fp);
    if(ok) *v=(uint64_t)x; return ok?0:-1;
}
static int choose_iface(char *out, size_t n) {
    const char *env=getenv("DISPLAY_NET_IFACE");
    const char *cands[]={env,"br-lan","eth0","wan","end0",NULL};
    char path[256]; struct stat st;
    for(int i=0;cands[i];++i) if(cands[i] && *cands[i]) {
        snprintf(path,sizeof(path),"/sys/class/net/%s",cands[i]);
        if(stat(path,&st)==0) { snprintf(out,n,"%s",cands[i]); return 0; }
    }
    snprintf(out,n,"eth0"); return -1;
}
static int get_ipv4(const char *iface, char *out, size_t n) {
    int fd=socket(AF_INET,SOCK_DGRAM,0); if(fd<0) return -1;
    struct ifreq ifr; memset(&ifr,0,sizeof(ifr)); snprintf(ifr.ifr_name,sizeof(ifr.ifr_name),"%s",iface);
    if(ioctl(fd,SIOCGIFADDR,&ifr)<0){ close(fd); return -1; }
    struct sockaddr_in *sin=(struct sockaddr_in*)&ifr.ifr_addr;
    snprintf(out,n,"%s",inet_ntoa(sin->sin_addr)); close(fd); return 0;
}
static int read_clients_arp(void) {
    FILE *fp=fopen("/proc/net/arp","r"); if(!fp) return 0;
    char line[512]; int count=0; fgets(line,sizeof(line),fp);
    while(fgets(line,sizeof(line),fp)) {
        unsigned flags=0; char ip[64],hw[64],mac[64],mask[64],dev[64];
        if(sscanf(line,"%63s %63s %x %63s %63s %63s",ip,hw,&flags,mac,mask,dev)==6 && (flags&0x2)) count++;
    }
    fclose(fp); return count;
}
static int read_mem_percent(void) {
    FILE *fp=fopen("/proc/meminfo","r"); if(!fp) return 0;
    long total=0, avail=0;
    char line[256];
    while(fgets(line,sizeof(line),fp)) {
        long val=0;
        if(sscanf(line,"MemTotal: %ld",&val)==1) total=val;
        else if(sscanf(line,"MemAvailable: %ld",&val)==1) avail=val;
    }
    fclose(fp); if(total<=0) return 0;
    if(avail<0) avail=0;
    return (int)lround(100.0*(double)(total-avail)/(double)total);
}
static float read_temp_c(void) {
    const char *paths[]={"/sys/class/thermal/thermal_zone0/temp","/sys/class/thermal/thermal_zone1/temp",NULL};
    for(int i=0;paths[i];++i){ FILE*fp=fopen(paths[i],"r"); if(fp){ long v; if(fscanf(fp,"%ld",&v)==1){fclose(fp); return v>1000? v/1000.0f:(float)v;} fclose(fp);} }
    return 0.0f;
}
static int read_cpu_percent(Metrics *m) {
    FILE *fp=fopen("/proc/stat","r"); if(!fp) return m->cpu;
    char cpu[16]; unsigned long long user,nice,sys,idle,iowait,irq,soft,steal;
    int rc=fscanf(fp,"%15s %llu %llu %llu %llu %llu %llu %llu %llu",cpu,&user,&nice,&sys,&idle,&iowait,&irq,&soft,&steal);
    fclose(fp); if(rc<5) return m->cpu;
    unsigned long long idle_all=idle+iowait;
    unsigned long long total=user+nice+sys+idle+iowait+irq+soft+steal;
    if(!m->prev_total){ m->prev_total=total; m->prev_idle=idle_all; return m->cpu; }
    unsigned long long dt=total-m->prev_total, di=idle_all-m->prev_idle;
    m->prev_total=total; m->prev_idle=idle_all;
    if(!dt) return m->cpu;
    return (int)lround(100.0*(double)(dt-di)/(double)dt);
}
static void format_rate(double bytes_s, char *out, size_t n) {
    if(bytes_s>=1e9) snprintf(out,n,"%.1fG",bytes_s/1e9);
    else if(bytes_s>=1e6) snprintf(out,n,"%.1fM",bytes_s/1e6);
    else if(bytes_s>=1e3) snprintf(out,n,"%.0fK",bytes_s/1e3);
    else snprintf(out,n,"%.0f",bytes_s);
}
static void metrics_init(Metrics *m) {
    memset(m,0,sizeof(*m));
    snprintf(m->ip,sizeof(m->ip),"--");
    snprintf(m->max_s,sizeof(m->max_s),"0"); snprintf(m->min_s,sizeof(m->min_s),"0");
    /* Start empty; never show a fabricated traffic ramp as live data. */
    for(int i=0;i<GRAPH_COLS;++i) m->graph[i]=0;
    m->iface[0]='\0'; m->iface_ready=0;
}
static void metrics_update(Metrics *m, double now) {
    time_t tt=time(NULL); struct tm tmv; localtime_r(&tt,&tmv);
    strftime(m->time_s,sizeof(m->time_s),"%H:%M",&tmv);
    strftime(m->sec_s,sizeof(m->sec_s),"%S",&tmv);
    strftime(m->date_s,sizeof(m->date_s),"%a %d %b",&tmv);
    m->cpu=read_cpu_percent(m); m->mem=read_mem_percent(); m->clients=read_clients_arp();
    float t=read_temp_c(); if(t>0.0f) m->temp=t;
    if (!m->iface_ready) { choose_iface(m->iface,sizeof(m->iface)); m->iface_ready=1; }
    const char *iface=m->iface;
    get_ipv4(iface,m->ip,sizeof(m->ip));
    char path[256]; uint64_t rx=0,tx=0;
    snprintf(path,sizeof(path),"/sys/class/net/%s/statistics/rx_bytes",iface);
    int rx_ok=read_u64_file(path,&rx)==0;
    snprintf(path,sizeof(path),"/sys/class/net/%s/statistics/tx_bytes",iface);
    int tx_ok=read_u64_file(path,&tx)==0;
    if (!rx_ok || !tx_ok) return;
    uint64_t total=rx+tx;
    if(!m->last_net_time){m->last_net_time=now; m->last_net_bytes=total; m->next_graph_sample=now+1.0;}
    if(now>=m->next_graph_sample){
        double dt=now-m->last_net_time;
        double rate=(dt>0 && total>=m->last_net_bytes)?(double)(total-m->last_net_bytes)/dt:0.0;
        m->last_net_time=now; m->last_net_bytes=total; m->next_graph_sample=now+1.0;
        for(int i=0;i<GRAPH_COLS-1;++i)m->graph[i]=m->graph[i+1];
        /* Map bytes/s to a useful 0..~100 MB/s display range.  The old
         * formula used raw bytes/s, so ordinary traffic saturated at 20. */
        int h=(int)lround(log10(rate/1024.0+1.0)*4.0); if(h<0)h=0; if(h>20)h=20; m->graph[GRAPH_COLS-1]=h;
        double maxr=0,minr=1e99; for(int i=0;i<GRAPH_COLS;++i){double rr=1024.0*(pow(10.0,m->graph[i]/4.0)-1.0); if(rr>maxr)maxr=rr;if(rr<minr)minr=rr;}
        format_rate(maxr,m->max_s,sizeof(m->max_s)); format_rate(minr,m->min_s,sizeof(m->min_s));
    }
}

static void render_dashboard(Canvas *c, Fonts *f, const Metrics *m, int page) {
    render_dashboard_background(c);
    vline(c,140,4,138,UI_LINE);
    vline(c,202,23,85,UI_LINE); hline(c,145,423,86,UI_LINE);
    vline(c,235,89,124,mix565(UI_BG_CENTER,UI_LINE,0.75f));
    vline(c,329,89,124,mix565(UI_BG_CENTER,UI_LINE,0.75f));

    draw_text_glow(c,f->semibold,18,7,3,"EDGEPI",UI_TEXT_BRIGHT,UI_GLOW);
    draw_text(c,f->semibold,5,88,6,"INTERNET",UI_TEXT_MID);
    draw_text(c,f->semibold,5,88,13,"ROUTER",UI_TEXT_DIM);
    draw_text(c,f->semibold,5,88,20,"STATION",UI_TEXT_DIM);

    char temp_s[16]; snprintf(temp_s,sizeof(temp_s),"%.1f",m->temp);
    /* Important: the temperature value is widened to occupy almost the full left panel. */
    draw_text_fit_width(c,f->number,51,5,26,128,temp_s,UI_TEXT_BRIGHT,UI_GLOW);
    draw_text(c,f->semibold,9,7,75,"°C",UI_TEXT_MID);
    draw_text(c,f->semibold,7,29,76,"SYSTEM",UI_TEXT_DIM);
    hline(c,8,132,88,mix565(UI_BG_CENTER,UI_LINE,0.85f));

    draw_text_glow(c,f->number,31,6,86,m->time_s,UI_TEXT_MAIN,UI_GLOW);
    rounded_rect(c,99,97,33,24,2,UI_SEC_BG);
    draw_text_center(c,f->semibold,17,99,132,98,m->sec_s,UI_SEC_TEXT);
    draw_text_center(c,f->semibold,5,95,136,124,"SECONDS",UI_TEXT_DIM);
    draw_text(c,f->semibold,5,7,131,m->date_s,UI_TEXT_DIM);
    (void)page;

    draw_text_glow(c,f->semibold,13,146,3,"LAST 240 SEC",UI_TEXT_BRIGHT,UI_GLOW);
    char maxline[24],minline[24]; snprintf(maxline,sizeof(maxline),"MAX:%s",m->max_s); snprintf(minline,sizeof(minline),"MIN:%s",m->min_s);
    draw_text_right(c,f->regular,7,422,3,maxline,UI_TEXT_MID);
    draw_text_right(c,f->regular,7,422,12,minline,UI_TEXT_MID);
    draw_text(c,f->semibold,7,153,29,"MAX",UI_TEXT_MID);
    draw_text(c,f->semibold,13,160,48,"T",UI_TEXT_DIM);
    draw_text(c,f->semibold,7,153,72,"MIN",UI_TEXT_MID);
    hline(c,202,419,79,mix565(UI_TEXT_MID,UI_TEXT_MAIN,0.35f));
    vline(c,202,29,79,mix565(UI_TEXT_MID,UI_TEXT_MAIN,0.35f));
    int gx=208,bottom=77;
    for(int j=0;j<GRAPH_COLS;++j){int h=m->graph[j]; int x=gx+j*7; if(x+5>419)break; for(int i=0;i<h;++i){int y=bottom-i*5; uint16_t col=i>=h-4?UI_GRAPH_TOP:UI_GRAPH_LOW; fill_rect(c,x,y-2,5,3,col);}}

    char cpu_s[16],mem_s[16],clients_s[16]; snprintf(cpu_s,sizeof(cpu_s),"%d%%",m->cpu); snprintf(mem_s,sizeof(mem_s),"%d%%",m->mem); snprintf(clients_s,sizeof(clients_s),"%d",m->clients);
    draw_text_center(c,f->semibold,7,145,235,91,"CPU",UI_TEXT_DIM);
    draw_text_center(c,f->number,20,145,235,100,cpu_s,UI_TEXT_MAIN);
    draw_text_center(c,f->semibold,7,235,329,91,"MEMORY",UI_TEXT_DIM);
    draw_text_center(c,f->number,20,235,329,100,mem_s,UI_TEXT_MAIN);
    draw_text_center(c,f->semibold,7,329,423,91,"CLIENTS",UI_TEXT_DIM);
    draw_text_center(c,f->number,20,329,423,100,clients_s,UI_TEXT_MAIN);
    draw_text(c,f->semibold,7,146,127,"CURRENT NETWORK",UI_TEXT_MID);
    draw_text(c,f->regular,6,252,127,"#WAN: ONLINE",UI_TEXT_MAIN);
    draw_text_right(c,f->regular,6,422,127,m->ip,UI_TEXT_MID);
}

typedef struct { float x,y,dist,ang; } BootTile;
static BootTile g_tiles[TILE_COUNT];
static void boot_tiles_init(void) {
    int k=0;
    for(int row=0;row<GRID_ROWS;++row) for(int col=0;col<GRID_COLS;++col){
        float x=2+col*TILE_PITCH+4, y=3+row*TILE_PITCH+4;
        float dx=x-LCD_W*0.5f, dy=y-LCD_H*0.5f;
        g_tiles[k++] = (BootTile){x,y,sqrtf(dx*dx+dy*dy),atan2f(dy,dx)};
    }
}
static float gauss(float d,float r,float w){float q=(d-r)/w;return expf(-0.5f*q*q);}
static float circular_wave(const BootTile *t,float time_s,float period,float strength){
    const float maxr=sqrtf((LCD_W*0.5f)*(LCD_W*0.5f)+(LCD_H*0.5f)*(LCD_H*0.5f))+18.0f;
    float phase=fmodf(time_s,period)/period;
    float r=phase*maxr;
    float d=t->dist+1.2f*sinf(t->ang*6.0f+time_s*1.2f);
    float main=gauss(d,r,6.8f);
    float er=fmodf(phase+0.58f,1.0f)*maxr;
    float echo=gauss(d,er,9.5f)*0.23f;
    return clamp01((main+echo)*strength);
}
static void render_boot_background(Canvas *c,float elapsed,float tft_mix){
    const float maxr=sqrtf((LCD_W*0.5f)*(LCD_W*0.5f)+(LCD_H*0.5f)*(LCD_H*0.5f));
    float global=smootherstep01((elapsed-0.20f)/1.05f);
    float front=global*1.18f;
    uint16_t black=rgb565(0,0,0);
    for(int y=0;y<LCD_H;++y)for(int x=0;x<LCD_W;++x){
        float dx=(float)x-LCD_W*0.5f,dy=(float)y-LCD_H*0.5f;
        float rn=sqrtf(dx*dx+dy*dy)/maxr;
        float local=smoothstep01((front-rn)/0.18f);
        float nx=dx/(LCD_W*0.5f),ny=dy/(LCD_H*0.5f);
        float glow=clamp01(1.0f-sqrtf(nx*nx*0.35f+ny*ny));
        uint16_t blue=mix565(BOOT_BG_EDGE,BOOT_BG_CENTER,0.50f+0.35f*glow);
        uint16_t boot=mix565(black,blue,local*global);
        uint16_t dash=mix565(UI_BG_EDGE,UI_BG_CENTER,0.22f+0.52f*glow);
        c->pix[y*LCD_W+x]=mix565(boot,dash,tft_mix);
    }
}
static void draw_core(Canvas *c,float time_s,float alpha){
    float phase=fmodf(time_s,1.28f)/1.28f;
    float sy=1.0f;
    if(phase>0.70f){if(phase<0.84f)sy=1.0f-0.18f*smoothstep01((phase-0.70f)/0.14f);else sy=0.82f+0.18f*smootherstep01((phase-0.84f)/0.16f);}
    int w=20,h=(int)lroundf(20*sy),x=LCD_W/2-w/2,y=LCD_H/2-h/2;
    uint16_t outer=mix565(BOOT_BG_CENTER,BOOT_CORE_LOW,alpha),inner=mix565(BOOT_CORE_LOW,BOOT_CORE_PEAK,alpha);
    rect_outline(c,x-1,y-1,w+2,h+2,1,outer);rect_outline(c,x,y,w,h,1,inner);
}
static void render_boot(Canvas *c,float elapsed){
    render_boot_background(c,elapsed,0.0f);
    float fade=smootherstep01((elapsed-0.28f)/0.85f);
    float wave_t=elapsed>1.05f?elapsed-1.05f:0.0f;
    for(int i=0;i<TILE_COUNT;++i){
        BootTile*t=&g_tiles[i];
        float amp=elapsed>1.05f?circular_wave(t,wave_t,1.28f,0.80f):0.0f;
        float appear=smootherstep01(fade*1.45f-t->dist/225.0f*0.32f);
        int size=amp>0.50f?6:5,yoff=amp>0.46f?-1:0;
        float light=amp*amp;
        uint16_t col=light<0.66f?mix565(BOOT_TILE_IDLE,BOOT_TILE_HIGH,light/0.66f):mix565(BOOT_TILE_HIGH,BOOT_TILE_PEAK,(light-0.66f)/0.34f);
        col=mix565(BOOT_BG_CENTER,col,appear);
        fill_rect(c,(int)t->x-size/2,(int)t->y-size/2+yoff,size,size,col);
    }
    draw_core(c,wave_t,fade);
}

typedef struct {float sx,sy,tx,ty,delay,dur; uint8_t role;} Particle;
typedef struct {int x,y;} Target;
static Particle g_particles[MAX_PARTICLES]; static int g_particle_count=0;
static unsigned rng_state=0x12345678u;
static unsigned prng(void){rng_state=rng_state*1664525u+1013904223u;return rng_state;}
static void build_particles(const Canvas *dash){
    Target all[MAX_TARGETS], graph[MAX_TARGETS], line[64]; int na=0,ng=0,nl=0;
    for(int y=3;y<LCD_H-3;y+=2)for(int x=3;x<LCD_W-3;x+=2){int lum=luma565(dash->pix[y*LCD_W+x]); if(lum>78&&na<MAX_TARGETS)all[na++]=(Target){x,y}; if(x>=202&&y>=23&&y<=86&&lum>95&&ng<MAX_TARGETS)graph[ng++]=(Target){x,y};}
    for(int y=5;y<=137;y+=3)line[nl++]=(Target){140,y};
    g_particle_count=0;
    int source_idx=0;
    for(int i=0;i<42&&g_particle_count<MAX_PARTICLES;++i){BootTile*s=&g_tiles[(source_idx+=13)%TILE_COUNT];Target t=line[i%nl];g_particles[g_particle_count++]=(Particle){s->x,s->y,t.x,t.y,0.04f+i*0.002f,0.46f+(i%7)*0.015f,1};}
    for(int i=0;i<120&&g_particle_count<MAX_PARTICLES&&ng>0;++i){BootTile*s=&g_tiles[(source_idx+=17)%TILE_COUNT];Target t=graph[prng()%ng];g_particles[g_particle_count++]=(Particle){s->x,s->y,t.x,t.y,0.11f+(i%17)*0.006f,0.48f+(i%11)*0.012f,2};}
    for(int i=0;i<190&&g_particle_count<MAX_PARTICLES&&na>0;++i){BootTile*s=&g_tiles[(source_idx+=7)%TILE_COUNT];Target t=all[prng()%na];g_particles[g_particle_count++]=(Particle){s->x,s->y,t.x,t.y,0.19f+(i%29)*0.007f,0.50f+(i%13)*0.012f,3};}
}
static void cubic_bezier(float t,float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,float *x,float*y){float u=1-t;*x=u*u*u*x0+3*u*u*t*x1+3*u*t*t*x2+t*t*t*x3;*y=u*u*u*y0+3*u*u*t*y1+3*u*t*t*y2+t*t*t*y3;}
static void overlay_dashboard_staged(Canvas *dst,const Canvas *dash,float structure,float detail,float value){
    for(int y=0;y<LCD_H;++y)for(int x=0;x<LCD_W;++x){
        int i=y*LCD_W+x,lum=luma565(dash->pix[i]);
        float a=0.0f;
        int structural=(x>=137&&x<=143)||(x>=198&&x<=205&&y>=20&&y<=88)||(y>=76&&y<=88&&x>=140)||(y>=86&&y<=126&&x>=140);
        if(structural)a=fmaxf(a,structure*(lum>16?0.88f:0.45f));
        float normal=clamp01(((float)lum-24.0f)/145.0f);
        float bright=clamp01(((float)lum-135.0f)/90.0f);
        a=fmaxf(a,normal*detail*(1.0f-bright));
        a=fmaxf(a,bright*value);
        dst->pix[i]=mix565(dst->pix[i],dash->pix[i],clamp01(a));
    }
}
static void morph_line(Canvas*c,float ax0,float ay0,float ax1,float ay1,float bx0,float by0,float bx1,float by1,float p,uint16_t col){
    int x0=(int)lroundf(lerpf(ax0,bx0,p)),y0=(int)lroundf(lerpf(ay0,by0,p));
    int x1=(int)lroundf(lerpf(ax1,bx1,p)),y1=(int)lroundf(lerpf(ay1,by1,p));
    draw_line(c,x0,y0,x1,y1,col);
}
static void render_transition(Canvas*c,const Canvas*dash,float elapsed){
    float p=clamp01(elapsed/(float)TRANSITION_DURATION);
    render_boot_background(c,2.50f,smoothstep01(p)*0.96f);
    const float maxr=sqrtf((LCD_W*0.5f)*(LCD_W*0.5f)+(LCD_H*0.5f)*(LCD_H*0.5f))+20.0f;
    float wr=smootherstep01(clamp01(p/0.72f))*maxr;
    uint16_t ringcol=mix565(BOOT_TILE_HIGH,UI_TEXT_MAIN,smoothstep01(p));
    float grid=1.0f-smoothstep01((p-0.10f)/0.68f);
    for(int i=0;i<TILE_COUNT;++i){
        BootTile*t=&g_tiles[i];float d=t->dist+0.8f*sinf(t->ang*6.0f+elapsed);
        float amp=gauss(d,wr,6.4f)*0.92f;
        if(amp<0.025f&&grid<0.025f)continue;
        uint16_t col=scale565(ringcol,clamp01(amp+0.12f*grid));int size=amp>0.46f?6:5;
        fill_rect(c,(int)t->x-size/2,(int)t->y-size/2,size,size,col);
    }

    float edge=smootherstep01((p-0.05f)/0.53f);
    float corealpha=1.0f-smoothstep01((p-0.03f)/0.28f);
    if(corealpha>0.01f)rect_outline(c,204,61,21,21,1,mix565(BOOT_BG_CENTER,BOOT_CORE_PEAK,corealpha));
    uint16_t skeleton=mix565(UI_LINE,UI_TEXT_MID,0.55f);
    morph_line(c,204,61,204,81,140,4,140,138,edge,skeleton);
    morph_line(c,224,61,224,81,202,29,202,79,edge,skeleton);
    morph_line(c,204,61,224,61,202,79,419,79,edge,skeleton);
    float bottom=smootherstep01((p-0.05f)/0.42f);
    morph_line(c,204,81,224,81,99,121,132,121,bottom,UI_TEXT_MAIN);
    float box=smootherstep01((p-0.40f)/0.24f);
    if(box>0.0f){int top=(int)lroundf(121.0f-24.0f*box);if(box>0.35f)rounded_rect(c,99,top,33,122-top,2,mix565(UI_BG_CENTER,UI_SEC_BG,smoothstep01((box-0.35f)/0.65f)));rect_outline(c,99,top,33,122-top,1,UI_SEC_BG);}

    /* Graph particles originate on the final circular ring, then lock to graph targets. */
    float gp=smootherstep01((p-0.18f)/0.48f);
    for(int i=0;i<g_particle_count;++i){Particle*pa=&g_particles[i];if(pa->role!=2)continue;
        float delay=fmodf((float)i*0.037f,0.16f);float u=smootherstep01((gp-delay)/(1.0f-delay));if(u<=0.0f)continue;
        float a=-0.72f+1.44f*(float)(i%61)/60.0f;float sx=LCD_W*0.5f+wr*cosf(a),sy=LCD_H*0.5f+wr*sinf(a);
        float x=lerpf(sx,pa->tx,u),y=lerpf(sy,pa->ty,u)-sinf((float)M_PI*u)*4.0f;int sz=u<0.78f?3:2;
        fill_rect(c,(int)lroundf(x)-sz/2,(int)lroundf(y)-sz/2,sz,sz,mix565(UI_GRAPH_LOW,UI_GRAPH_TOP,u));
    }

    float structure=smootherstep01((p-0.28f)/0.30f);
    float detail=smootherstep01((p-0.44f)/0.34f);
    float value=smootherstep01((p-0.58f)/0.30f);
    overlay_dashboard_staged(c,dash,structure,detail,value);
}

static void render_focus(Canvas *c,const Canvas *dash,float p){
    memcpy(c->pix,dash->pix,LCD_W*LCD_H*2);
    if(p<0.64f){for(int y=0;y<LCD_H;++y)for(int x=1;x<LCD_W-1;++x){uint16_t src=dash->pix[y*LCD_W+x];int lum=luma565(src);if(lum>55){blend_px(c,x-1,y,UI_GLOW,38);blend_px(c,x+1,y,UI_TEXT_BRIGHT,28);}}}
}

static void sleep_frame(double frame_start){
    double target=frame_start+1.0/TARGET_FPS, now=mono_seconds(); double left=target-now;
    if(left>0){struct timespec ts;ts.tv_sec=(time_t)left;ts.tv_nsec=(long)((left-ts.tv_sec)*1e9);nanosleep(&ts,NULL);}
}

int main(int argc,char**argv){
    const char *mode=argc>1?argv[1]:"boot"; int page=1; double timeout=argc>3?atof(argv[3]):BOOT_DEFAULT_TIMEOUT;
    struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_handler=handle_signal;sigemptyset(&sa.sa_mask);sigaction(SIGTERM,&sa,NULL);sigaction(SIGINT,&sa,NULL);sigaction(SIGUSR1,&sa,NULL);
    UI_BG_EDGE=rgb565(7,13,19);UI_BG_CENTER=rgb565(22,31,40);UI_PANEL=rgb565(22,31,39);UI_PANEL_DARK=rgb565(14,22,29);UI_TEXT_MAIN=rgb565(220,230,225);UI_TEXT_BRIGHT=rgb565(239,245,233);UI_TEXT_MID=rgb565(164,178,178);UI_TEXT_DIM=rgb565(102,117,120);UI_LINE=rgb565(73,88,98);UI_GLOW=rgb565(119,203,220);UI_GRAPH_TOP=rgb565(214,226,220);UI_GRAPH_LOW=rgb565(128,149,153);UI_SEC_BG=rgb565(222,232,224);UI_SEC_TEXT=rgb565(29,40,44);
    BOOT_BG_EDGE=rgb565(0,2,10);BOOT_BG_CENTER=rgb565(1,12,32);BOOT_TILE_IDLE=rgb565(3,15,31);BOOT_TILE_HIGH=rgb565(18,139,199);BOOT_TILE_PEAK=rgb565(96,216,248);BOOT_CORE_LOW=rgb565(8,78,130);BOOT_CORE_PEAK=rgb565(145,235,255);
    FbCtx fb; if(fb_open(&fb,"/dev/fb0"))return 1;
    Canvas frame={calloc(LCD_W*LCD_H,sizeof(uint16_t)),LCD_W,LCD_H};Canvas dash={calloc(LCD_W*LCD_H,sizeof(uint16_t)),LCD_W,LCD_H};if(!frame.pix||!dash.pix){fprintf(stderr,"out of memory\n");return 1;}
    Fonts fonts;if(fonts_init(&fonts)){fprintf(stderr,"font init failed\n");return 1;}
    Metrics metrics;metrics_init(&metrics);metrics_update(&metrics,mono_seconds());boot_tiles_init();
    double started=mono_seconds(),last_metrics=0,transition_started=0,focus_started=0;enum{ST_BOOT,ST_TRANS,ST_FOCUS,ST_DASH}state=(!strcmp(mode,"dashboard") || isdigit((unsigned char)mode[0]))?ST_DASH:ST_BOOT;
    if(state==ST_DASH){render_dashboard(&dash,&fonts,&metrics,page);memcpy(frame.pix,dash.pix,LCD_W*LCD_H*2);}
    while(g_running){double fs=mono_seconds(),elapsed=fs-started;if(fs-last_metrics>=1.0){metrics_update(&metrics,fs);last_metrics=fs;}
        if(state==ST_BOOT){render_boot(&frame,(float)elapsed);if(g_ready||elapsed>=timeout){render_dashboard(&dash,&fonts,&metrics,page);build_particles(&dash);transition_started=fs;state=ST_TRANS;}}
        else if(state==ST_TRANS){double te=fs-transition_started;render_transition(&frame,&dash,(float)te);if(te>=TRANSITION_DURATION){focus_started=fs;state=ST_FOCUS;}}
        else if(state==ST_FOCUS){double fe=fs-focus_started;render_focus(&frame,&dash,smoothstep01((float)(fe/FOCUS_DURATION)));if(fe>=FOCUS_DURATION)state=ST_DASH;}
        else {render_dashboard(&frame,&fonts,&metrics,page);memcpy(dash.pix,frame.pix,LCD_W*LCD_H*2);}fb_present(&fb,&frame);write_heartbeat();sleep_frame(fs);
    }
    fonts_close(&fonts);free(frame.pix);free(dash.pix);fb_close(&fb);return 0;
}
