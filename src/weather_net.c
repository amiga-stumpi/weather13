#include "weather_net.h"
#include "amitcp13/bsdsocket.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <string.h>

#define W13_HTTP_BUF_SIZE 18000
#define W13_REQ_SIZE 768
#define W13_GEO_HOST "geocoding-api.open-meteo.com"
#define W13_API_HOST "api.open-meteo.com"
#define W13_HTTP_PORT 80

static struct Library *g_sock_base;
static struct Amitcp13BsdFdSet g_rfds;
static struct Amitcp13BsdFdSet g_wfds;
static struct Amitcp13BsdTimeVal g_tv;
static ULONG g_wait_sigs;
static int g_one;
static int g_so_error;
static int g_so_error_len;
static struct Amitcp13BsdSockAddrIn g_addr;
static char g_http_buf[W13_HTTP_BUF_SIZE];

static int text_len(const char *s)
{
    int n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static void set_status(char *status, int status_size, const char *s)
{
    int i = 0;
    if (!status || status_size <= 0)
        return;
    while (s && s[i] && i < status_size - 1) {
        status[i] = s[i];
        ++i;
    }
    status[i] = 0;
}

static void copy_text(char *dst, int dst_size, const char *src)
{
    int i = 0;
    if (!dst || dst_size <= 0)
        return;
    while (src && src[i] && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void append_text(char *dst, int dst_size, const char *src)
{
    int p = text_len(dst);
    int i = 0;
    while (src && src[i] && p < dst_size - 1)
        dst[p++] = src[i++];
    dst[p] = 0;
}

static void append_char(char *dst, int dst_size, char c)
{
    int p = text_len(dst);
    if (p < dst_size - 1) {
        dst[p++] = c;
        dst[p] = 0;
    }
}

static void append_int(char *dst, int dst_size, long value)
{
    char rev[12];
    int r = 0;
    unsigned long v;
    if (value < 0) {
        append_char(dst, dst_size, '-');
        v = (unsigned long)(-value);
    } else {
        v = (unsigned long)value;
    }
    if (v == 0)
        rev[r++] = '0';
    while (v && r < (int)sizeof(rev)) {
        rev[r++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (r > 0)
        append_char(dst, dst_size, rev[--r]);
}

static void append_coord(char *dst, int dst_size, long e6)
{
    long whole;
    long frac;
    if (e6 < 0) {
        append_char(dst, dst_size, '-');
        e6 = -e6;
    }
    whole = e6 / 1000000L;
    frac = e6 % 1000000L;
    append_int(dst, dst_size, whole);
    append_char(dst, dst_size, '.');
    if (frac < 100000L) append_char(dst, dst_size, '0');
    if (frac < 10000L) append_char(dst, dst_size, '0');
    if (frac < 1000L) append_char(dst, dst_size, '0');
    if (frac < 100L) append_char(dst, dst_size, '0');
    if (frac < 10L) append_char(dst, dst_size, '0');
    append_int(dst, dst_size, frac);
}


static void append_pct_byte(char *dst, int dst_size, int *j, UBYTE c)
{
    static const char hex[] = "0123456789ABCDEF";
    if (*j + 3 < dst_size) {
        dst[(*j)++] = '%';
        dst[(*j)++] = hex[(c >> 4) & 0x0f];
        dst[(*j)++] = hex[c & 0x0f];
    }
}

static void append_utf8_pct_latin1(char *dst, int dst_size, int *j, UBYTE c)
{
    append_pct_byte(dst, dst_size, j, (UBYTE)(0xc0 | (c >> 6)));
    append_pct_byte(dst, dst_size, j, (UBYTE)(0x80 | (c & 0x3f)));
}

static void url_encode_query(const char *src, char *dst, int dst_size)
{
    int i = 0;
    int j = 0;
    UBYTE c;

    while (src && src[i] && j + 1 < dst_size) {
        c = (UBYTE)src[i++];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            dst[j++] = (char)c;
        } else if (c == ' ') {
            dst[j++] = '+';
        } else if (c == 0xe4 || c == 0xf6 || c == 0xfc || c == 0xc4 || c == 0xd6 || c == 0xdc || c == 0xdf) {
            append_utf8_pct_latin1(dst, dst_size, &j, c);
        } else {
            append_pct_byte(dst, dst_size, &j, c);
        }
    }
    dst[j] = 0;
}

static int call_socket(struct Library *base, int domain, int type, int protocol)
{
    register int d0 __asm("d0") = domain;
    register int d1 __asm("d1") = type;
    register int d2 __asm("d2") = protocol;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-30:W)" : "+r"(d0), "+r"(d1), "+r"(d2) : "r"(a6) : "a0", "a1", "cc", "memory");
    return d0;
}

static int call_connect(struct Library *base, int fd, const struct Amitcp13BsdSockAddr *addr, int addrlen)
{
    register int d0 __asm("d0") = fd;
    register const struct Amitcp13BsdSockAddr *a0 __asm("a0") = addr;
    register int d1 __asm("d1") = addrlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-54:W)" : "+r"(d0), "+r"(a0), "+r"(d1) : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

static int call_send(struct Library *base, int fd, const void *buf, int len)
{
    register int d0 __asm("d0") = fd;
    register const void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = 0;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-66:W)" : "+r"(d0), "+r"(a0), "+r"(d1), "+r"(d2) : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

static int call_recv(struct Library *base, int fd, void *buf, int len)
{
    register int d0 __asm("d0") = fd;
    register void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = 0;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-78:W)" : "+r"(d0), "+r"(a0), "+r"(d1), "+r"(d2) : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

static int call_ioctl(struct Library *base, int fd, ULONG request, void *argp)
{
    register int d0 __asm("d0") = fd;
    register ULONG d1 __asm("d1") = request;
    register void *a0 __asm("a0") = argp;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-114:W)" : "+r"(d0), "+r"(d1), "+r"(a0) : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

static int call_close_socket(struct Library *base, int fd)
{
    register int d0 __asm("d0") = fd;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-120:W)" : "+r"(d0) : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static int call_waitselect(struct Library *base, int nfds, struct Amitcp13BsdFdSet *readfds,
                           struct Amitcp13BsdFdSet *writefds, const struct Amitcp13BsdTimeVal *timeout)
{
    register int d0 __asm("d0") = nfds;
    register ULONG *d1 __asm("d1") = &g_wait_sigs;
    register struct Amitcp13BsdFdSet *a0 __asm("a0") = readfds;
    register struct Amitcp13BsdFdSet *a1 __asm("a1") = writefds;
    register struct Amitcp13BsdFdSet *a2 __asm("a2") = 0;
    register const struct Amitcp13BsdTimeVal *a3 __asm("a3") = timeout;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-126:W)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3) : "r"(a6) : "cc", "memory");
    return d0;
}

static int call_errno(struct Library *base)
{
    register int d0 __asm("d0");
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-162:W)" : "=r"(d0) : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static struct hostent *call_gethostbyname(struct Library *base, const char *name)
{
    register const char *a0 __asm("a0") = name;
    register struct Library *a6 __asm("a6") = base;
    register struct hostent *d0 __asm("d0");
    __asm volatile ("jsr a6@(-210:W)" : "=r"(d0), "+r"(a0) : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}

static int call_getsockopt(struct Library *base, int fd, int level, int optname, void *optval, int *optlen)
{
    register int d0 __asm("d0") = fd;
    register int d1 __asm("d1") = level;
    register int d2 __asm("d2") = optname;
    register void *a0 __asm("a0") = optval;
    register int *a1 __asm("a1") = optlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-96:W)" : "+r"(d0), "+r"(d1), "+r"(d2), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    return d0;
}

static int wait_fd(int fd, int write, LONG sec)
{
    AMITCP13_BSD_FD_ZERO(&g_rfds);
    AMITCP13_BSD_FD_ZERO(&g_wfds);
    if (write)
        AMITCP13_BSD_FD_SET(fd, &g_wfds);
    else
        AMITCP13_BSD_FD_SET(fd, &g_rfds);
    g_tv.tv_sec = sec;
    g_tv.tv_usec = 0;
    g_wait_sigs = 0;
    return call_waitselect(g_sock_base, fd + 1, write ? 0 : &g_rfds, write ? &g_wfds : 0, &g_tv);
}

static int connect_host(const char *host)
{
    struct hostent *he;
    int fd;
    int r;
    int err;

    if (!g_sock_base) {
        g_sock_base = OpenLibrary((STRPTR)"bsdsocket.library", 0);
        if (!g_sock_base)
            return -1;
    }
    he = call_gethostbyname(g_sock_base, host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
        return -2;
    fd = call_socket(g_sock_base, AMITCP13_AF_INET, AMITCP13_SOCK_STREAM, AMITCP13_IPPROTO_TCP);
    if (fd < 0)
        return -3;
    g_one = 1;
    call_ioctl(g_sock_base, fd, AMITCP13_FIONBIO, &g_one);
    g_addr.sin_len = sizeof(g_addr);
    g_addr.sin_family = AMITCP13_AF_INET;
    g_addr.sin_port = W13_HTTP_PORT;
    g_addr.sin_addr.s_addr = *(ULONG *)he->h_addr_list[0];
    r = call_connect(g_sock_base, fd, (const struct Amitcp13BsdSockAddr *)&g_addr, sizeof(g_addr));
    if (r < 0) {
        err = call_errno(g_sock_base);
        if (err != AMITCP13_EINPROGRESS && err != AMITCP13_EALREADY) {
            call_close_socket(g_sock_base, fd);
            return -4;
        }
        r = wait_fd(fd, 1, 15);
        if (r <= 0 || !AMITCP13_BSD_FD_ISSET(fd, &g_wfds)) {
            call_close_socket(g_sock_base, fd);
            return -5;
        }
        g_so_error = -1;
        g_so_error_len = sizeof(g_so_error);
        r = call_getsockopt(g_sock_base, fd, AMITCP13_SOL_SOCKET, AMITCP13_SO_ERROR, &g_so_error, &g_so_error_len);
        if (r < 0 || g_so_error != 0) {
            call_close_socket(g_sock_base, fd);
            return -6;
        }
    }
    return fd;
}

static void build_request(char *req, int req_size, const char *host, const char *path)
{
    req[0] = 0;
    append_text(req, req_size, "GET ");
    append_text(req, req_size, path);
    append_text(req, req_size, " HTTP/1.0\r\nHost: ");
    append_text(req, req_size, host);
    append_text(req, req_size, "\r\nUser-Agent: Weather13/1.1 (AmigaOS 1.3)\r\nConnection: close\r\n\r\n");
}

static int http_fetch(const char *host, const char *path, char *status, int status_size)
{
    char req[W13_REQ_SIZE];
    int fd;
    int r;
    int used = 0;
    int idle = 0;

    fd = connect_host(host);
    if (fd < 0) {
        set_status(status, status_size, "Connect failed");
        return fd;
    }
    build_request(req, sizeof(req), host, path);
    r = call_send(g_sock_base, fd, req, text_len(req));
    if (r < 0) {
        call_close_socket(g_sock_base, fd);
        set_status(status, status_size, "HTTP send failed");
        return -7;
    }
    while (used + 1 < W13_HTTP_BUF_SIZE && idle < 20) {
        r = wait_fd(fd, 0, 1);
        if (r <= 0) {
            ++idle;
            continue;
        }
        if (!AMITCP13_BSD_FD_ISSET(fd, &g_rfds)) {
            ++idle;
            continue;
        }
        r = call_recv(g_sock_base, fd, g_http_buf + used, W13_HTTP_BUF_SIZE - used - 1);
        if (r > 0) {
            used += r;
            idle = 0;
        } else if (r == 0) {
            break;
        } else {
            int err = call_errno(g_sock_base);
            if (err == AMITCP13_EWOULDBLOCK || err == AMITCP13_EAGAIN || err == AMITCP13_EINPROGRESS) {
                ++idle;
                continue;
            }
            call_close_socket(g_sock_base, fd);
            set_status(status, status_size, "HTTP recv failed");
            return -8;
        }
    }
    g_http_buf[used] = 0;
    call_close_socket(g_sock_base, fd);
    return used;
}

static char *find_text(char *start, const char *needle)
{
    int n = text_len(needle);
    char *p;
    if (!start || !needle || !needle[0])
        return 0;
    for (p = start; *p; ++p) {
        int i;
        for (i = 0; i < n && p[i] == needle[i]; ++i) ;
        if (i == n)
            return p;
    }
    return 0;
}

static long parse_scaled(char *p, int scale)
{
    long value = 0;
    long frac = 0;
    int frac_scale = scale;
    int neg = 0;

    while (p && *p && *p != '-' && (*p < '0' || *p > '9'))
        ++p;
    if (!p)
        return 0;
    if (*p == '-') {
        neg = 1;
        ++p;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        ++p;
    }
    value *= scale;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && frac_scale > 1) {
            frac_scale /= 10;
            frac += (*p - '0') * frac_scale;
            ++p;
        }
    }
    value += frac;
    return neg ? -value : value;
}

static char *json_field(char *start, const char *field)
{
    char pat[48];
    pat[0] = 0;
    append_char(pat, sizeof(pat), '"');
    append_text(pat, sizeof(pat), field);
    append_text(pat, sizeof(pat), "\":");
    return find_text(start, pat);
}


static int lower_char(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static int text_equals_ci(const char *a, const char *b)
{
    int i = 0;
    if (!a || !b)
        return 0;
    while (a[i] && b[i]) {
        if (lower_char((UBYTE)a[i]) != lower_char((UBYTE)b[i]))
            return 0;
        ++i;
    }
    return a[i] == 0 && b[i] == 0;
}


static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void append_latin1_char(char *dst, int dst_size, UBYTE c)
{
    if (c == 0xe4) append_text(dst, dst_size, "ae");
    else if (c == 0xf6) append_text(dst, dst_size, "oe");
    else if (c == 0xfc) append_text(dst, dst_size, "ue");
    else if (c == 0xc4) append_text(dst, dst_size, "Ae");
    else if (c == 0xd6) append_text(dst, dst_size, "Oe");
    else if (c == 0xdc) append_text(dst, dst_size, "Ue");
    else if (c == 0xdf) append_text(dst, dst_size, "ss");
    else if (c >= 32 && c < 127) append_char(dst, dst_size, (char)c);
}

static void append_utf8_json_char(char *dst, int dst_size, char **pp)
{
    UBYTE c = (UBYTE)**pp;
    if (c == 0xc3 && (*pp)[1]) {
        ++(*pp);
        append_latin1_char(dst, dst_size, (UBYTE)**pp);
    } else if (c < 128) {
        append_char(dst, dst_size, (char)c);
    } else {
        append_latin1_char(dst, dst_size, c);
    }
}

static char *json_string_field(char *start, const char *field, char *dst, int dst_size)
{
    char *p = json_field(start, field);
    if (!p || !dst || dst_size <= 0)
        return 0;
    dst[0] = 0;
    while (*p && *p != ':')
        ++p;
    if (*p == ':')
        ++p;
    while (*p && *p != '"')
        ++p;
    if (*p != '"')
        return 0;
    ++p;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            ++p;
            if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                int h1 = hex_value(p[1]);
                int h2 = hex_value(p[2]);
                int h3 = hex_value(p[3]);
                int h4 = hex_value(p[4]);
                if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                    UWORD code = (UWORD)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                    if (code <= 255)
                        append_latin1_char(dst, dst_size, (UBYTE)code);
                    p += 4;
                }
            } else if (*p == 'n' || *p == 'r' || *p == 't') {
                ;
            } else {
                append_char(dst, dst_size, *p);
            }
        } else {
            append_utf8_json_char(dst, dst_size, &p);
        }
        ++p;
    }
    return *p == '"' ? p + 1 : p;
}


static char *json_object(char *start, const char *field)
{
    char *p = json_field(start, field);
    if (!p)
        return 0;
    while (*p && *p != '{')
        ++p;
    return *p == '{' ? p + 1 : 0;
}

static int parse_field_scaled(char *start, const char *field, int scale)
{
    char *p = json_field(start, field);
    if (!p)
        return 0;
    while (*p && *p != ':')
        ++p;
    if (*p == ':')
        ++p;
    return (int)parse_scaled(p, scale);
}

static char *json_array(char *start, const char *field)
{
    char *p = json_field(start, field);
    if (!p)
        return 0;
    while (*p && *p != '[')
        ++p;
    return *p == '[' ? p + 1 : 0;
}

static int parse_array_value(char *start, const char *field, int index, int scale)
{
    char *p = json_array(start, field);
    int i;
    if (!p)
        return 0;
    for (i = 0; i < index; ++i) {
        while (*p && *p != ',')
            ++p;
        if (*p == ',')
            ++p;
    }
    return (int)parse_scaled(p, scale);
}

static int map_weather_code(int code)
{
    if (code == 0 || code == 1)
        return W13_WEATHER_SUN;
    if (code == 2)
        return W13_WEATHER_PARTLY;
    if (code == 3)
        return W13_WEATHER_CLOUD;
    if (code == 45 || code == 48)
        return W13_WEATHER_FOG;
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
        return W13_WEATHER_RAIN;
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86))
        return W13_WEATHER_SNOW;
    if (code >= 95 && code <= 99)
        return W13_WEATHER_STORM;
    return W13_WEATHER_UNKNOWN;
}

static const char *condition_from_code(int code)
{
    switch (map_weather_code(code)) {
    case W13_WEATHER_SUN: return "Sonnig";
    case W13_WEATHER_PARTLY: return "Leicht bewoelkt";
    case W13_WEATHER_CLOUD: return "Bewoelkt";
    case W13_WEATHER_RAIN: return "Regen";
    case W13_WEATHER_STORM: return "Gewitter";
    case W13_WEATHER_FOG: return "Nebel";
    case W13_WEATHER_SNOW: return "Schnee";
    default: return "Unbekannt";
    }
}



static int make_german_search_variant(const char *src, char *dst, int dst_size)
{
    int i = 0;
    int j = 0;
    int changed = 0;
    UBYTE c;

    if (!dst || dst_size <= 0)
        return 0;
    while (src && src[i] && j < dst_size - 1) {
        c = (UBYTE)src[i];
        if ((c == 'A' || c == 'O' || c == 'U' || c == 'a' || c == 'o' || c == 'u') &&
            (src[i + 1] == 'e' || src[i + 1] == 'E')) {
            dst[j++] = (char)c;
            i += 2;
            changed = 1;
        } else if (c == 0xe4) {
            dst[j++] = 'a';
            ++i;
            changed = 1;
        } else if (c == 0xf6) {
            dst[j++] = 'o';
            ++i;
            changed = 1;
        } else if (c == 0xfc) {
            dst[j++] = 'u';
            ++i;
            changed = 1;
        } else if (c == 0xc4) {
            dst[j++] = 'A';
            ++i;
            changed = 1;
        } else if (c == 0xd6) {
            dst[j++] = 'O';
            ++i;
            changed = 1;
        } else if (c == 0xdc) {
            dst[j++] = 'U';
            ++i;
            changed = 1;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
    return changed && dst[0];
}


static void build_location_label(char *dst, int dst_size, const char *name, const char *admin1, const char *country)
{
    dst[0] = 0;
    append_text(dst, dst_size, name);
    if (admin1 && admin1[0]) {
        append_text(dst, dst_size, " - ");
        append_text(dst, dst_size, admin1);
    }
    if (country && country[0]) {
        if (admin1 && admin1[0])
            append_text(dst, dst_size, ", ");
        else
            append_text(dst, dst_size, " - ");
        append_text(dst, dst_size, country);
    }
}

static int search_locations_query(const char *query, char names[][32], char labels[][64], int max_names, char *status, int status_size)
{
    char enc[96];
    char path[192];
    char *p;
    int r;
    int count = 0;

    url_encode_query(query, enc, sizeof(enc));
    path[0] = 0;
    append_text(path, sizeof(path), "/v1/search?name=");
    append_text(path, sizeof(path), enc);
    append_text(path, sizeof(path), "&count=3&language=de&format=json");
    set_status(status, status_size, "Searching location...");
    r = http_fetch(W13_GEO_HOST, path, status, status_size);
    if (r <= 0)
        return 0;
    p = find_text(g_http_buf, "\"results\"");
    if (!p)
        return 0;
    while (count < max_names) {
        char admin1[32];
        char country[32];
        char *next = json_string_field(p, "name", names[count], 32);
        if (!next || !names[count][0])
            break;
        admin1[0] = 0;
        country[0] = 0;
        json_string_field(next, "admin1", admin1, sizeof(admin1));
        json_string_field(next, "country", country, sizeof(country));
        if (labels)
            build_location_label(labels[count], 64, names[count], admin1, country);
        ++count;
        p = next;
    }
    return count;
}

static int names_contain_match(const char *location, char names[][32], int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (text_equals_ci(location, names[i]))
            return 1;
    }
    return 0;
}

int W13_SearchLocations(const char *location, char names[][32], char labels[][64], int max_names, char *status, int status_size)
{
    char variant[64];
    int count;

    if (!location || !location[0] || !names || max_names <= 0) {
        set_status(status, status_size, "No location set");
        return 0;
    }
    count = search_locations_query(location, names, labels, max_names, status, status_size);
    if (count > 0 && names_contain_match(location, names, count)) {
        set_status(status, status_size, "Location found");
        return count;
    }
    if (make_german_search_variant(location, variant, sizeof(variant))) {
        int retry_count = search_locations_query(variant, names, labels, max_names, status, status_size);
        if (retry_count > 0)
            count = retry_count;
    }
    if (count > 0) {
        if (names_contain_match(location, names, count))
            set_status(status, status_size, "Location found");
        else
            set_status(status, status_size, "Similar locations found");
    } else {
        set_status(status, status_size, "Location not found");
    }
    return count;
}


static int geocode_location_query(const char *query, long *lat_e6, long *lon_e6, char *name, int name_size, char *status, int status_size)
{
    char enc[96];
    char path[192];
    int r;

    url_encode_query(query, enc, sizeof(enc));
    path[0] = 0;
    append_text(path, sizeof(path), "/v1/search?name=");
    append_text(path, sizeof(path), enc);
    append_text(path, sizeof(path), "&count=1&language=de&format=json");
    set_status(status, status_size, "Searching location...");
    r = http_fetch(W13_GEO_HOST, path, status, status_size);
    if (r <= 0)
        return 0;
    if (!find_text(g_http_buf, "\"results\""))
        return 0;
    *lat_e6 = parse_field_scaled(g_http_buf, "latitude", 1000000);
    *lon_e6 = parse_field_scaled(g_http_buf, "longitude", 1000000);
    if (!json_string_field(g_http_buf, "name", name, name_size) || !name[0])
        copy_text(name, name_size, query);
    return 1;
}

static int geocode_location(const char *location, long *lat_e6, long *lon_e6, char *name, int name_size, char *status, int status_size)
{
    char variant[64];

    if (!geocode_location_query(location, lat_e6, lon_e6, name, name_size, status, status_size)) {
        if (make_german_search_variant(location, variant, sizeof(variant)) &&
            geocode_location_query(variant, lat_e6, lon_e6, name, name_size, status, status_size))
            return 1;
        set_status(status, status_size, "Location not found");
        return 0;
    }
    if (!text_equals_ci(location, name) && make_german_search_variant(location, variant, sizeof(variant))) {
        long retry_lat = 0;
        long retry_lon = 0;
        char retry_name[32];
        retry_name[0] = 0;
        if (geocode_location_query(variant, &retry_lat, &retry_lon, retry_name, sizeof(retry_name), status, status_size)) {
            *lat_e6 = retry_lat;
            *lon_e6 = retry_lon;
            copy_text(name, name_size, retry_name);
        }
    }
    return 1;
}

static int fetch_forecast(long lat_e6, long lon_e6, WeatherData *data, char *status, int status_size)
{
    char path[640];
    char *current;
    char *daily;
    int r;
    int i;
    int wcode;

    path[0] = 0;
    append_text(path, sizeof(path), "/v1/forecast?latitude=");
    append_coord(path, sizeof(path), lat_e6);
    append_text(path, sizeof(path), "&longitude=");
    append_coord(path, sizeof(path), lon_e6);
    append_text(path, sizeof(path), "&current=temperature_2m,apparent_temperature,relative_humidity_2m,pressure_msl,wind_speed_10m,wind_direction_10m,weather_code");
    append_text(path, sizeof(path), "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,wind_speed_10m_max,wind_direction_10m_dominant&timezone=auto&forecast_days=3");
    set_status(status, status_size, "Fetching weather...");
    r = http_fetch(W13_API_HOST, path, status, status_size);
    if (r <= 0)
        return 0;

    current = json_object(g_http_buf, "current");
    daily = json_object(g_http_buf, "daily");
    if (!current || !daily) {
        set_status(status, status_size, "Weather parse failed");
        return 0;
    }

    data->temp_tenths = parse_field_scaled(current, "temperature_2m", 10);
    data->apparent_temp_tenths = parse_field_scaled(current, "apparent_temperature", 10);
    data->humidity = parse_field_scaled(current, "relative_humidity_2m", 1);
    data->pressure = parse_field_scaled(current, "pressure_msl", 1);
    data->wind_speed = parse_field_scaled(current, "wind_speed_10m", 1);
    data->wind_dir_deg = parse_field_scaled(current, "wind_direction_10m", 1);
    wcode = parse_field_scaled(current, "weather_code", 1);
    data->weather_code = map_weather_code(wcode);
    copy_text(data->condition, sizeof(data->condition), condition_from_code(wcode));
    copy_text(data->updated, sizeof(data->updated), "Online");

    for (i = 0; i < 3; ++i) {
        if (i == 0)
            copy_text(data->forecast[i].name, sizeof(data->forecast[i].name), "Heute");
        else if (i == 1)
            copy_text(data->forecast[i].name, sizeof(data->forecast[i].name), "Morgen");
        else
            copy_text(data->forecast[i].name, sizeof(data->forecast[i].name), "Uebermorgen");
        wcode = parse_array_value(daily, "weather_code", i, 1);
        data->forecast[i].weather_code = map_weather_code(wcode);
        data->forecast[i].temp_day = parse_array_value(daily, "temperature_2m_max", i, 1);
        data->forecast[i].temp_night = parse_array_value(daily, "temperature_2m_min", i, 1);
        data->forecast[i].precip_probability = parse_array_value(daily, "precipitation_probability_max", i, 1);
        data->forecast[i].wind_speed_max = parse_array_value(daily, "wind_speed_10m_max", i, 1);
        data->forecast[i].wind_dir_deg = parse_array_value(daily, "wind_direction_10m_dominant", i, 1);
    }
    set_status(status, status_size, "Weather updated");
    return 1;
}

int W13_FetchWeatherForLocation(const char *location, WeatherData *data, char *status, int status_size)
{
    long lat_e6 = 0;
    long lon_e6 = 0;
    char name[32];

    if (!location || !location[0] || !data) {
        set_status(status, status_size, "No location set");
        return 0;
    }
    name[0] = 0;
    if (!geocode_location(location, &lat_e6, &lon_e6, name, sizeof(name), status, status_size))
        return 0;
    copy_text(data->location, sizeof(data->location), name);
    return fetch_forecast(lat_e6, lon_e6, data, status, status_size);
}
