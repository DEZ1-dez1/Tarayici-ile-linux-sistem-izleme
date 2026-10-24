#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define DEFAULT_PORT 8080
#define MAX_PROC_READ (512 * 1024)  // tek bir dosya için üst sınır

typedef struct {
    const char *group;
    const char *path;
    const char *title;
} Source;

// Not: Numaralar (1 tabanlı) URL'lerde kullanılır: /view?m=NUMARA
static const Source SOURCES[] = {
    /*  1 */ {"Sistem",       "/proc/loadavg",        "İşlemci Yükü"},
    /*  2 */ {"Bellek",       "/proc/meminfo",        "RAM / Bellek Bilgileri"},
    /*  3 */ {"İşlemci",      "/proc/cpuinfo",        "İşlemci Detayları"},
    /*  4 */ {"Sistem",       "/proc/uptime",         "Sistem Çalışma Süresi"},
    /*  5 */ {"Çekirdek",     "/proc/version",        "Çekirdek Versiyonu"},
    /*  6 */ {"Ağ",           "/proc/net/dev",        "Ağ Trafiği"},
    /*  7 */ {"İşlemci",      "/proc/stat",           "CPU İstatistikleri"},
    /*  8 */ {"Ağ",           "/proc/net/tcp",        "Aktif TCP Bağlantıları"},
    /*  9 */ {"Sistem",       "/proc/self/status",    "Sunucu Öz Kaynakları"},
    /* 10 */ {"Çekirdek",     "/proc/cmdline",        "Çekirdek Komut Satırı"},
    /* 11 */ {"Disk",         "/proc/mounts",         "Bağlı Dosya Sistemleri"},
    /* 12 */ {"Çekirdek",     "/proc/modules",        "Yüklü Çekirdek Modülleri"},
    /* 13 */ {"Bellek",       "/proc/zoneinfo",       "Bellek Bölgeleri"},
    /* 14 */ {"Bellek",       "/proc/vmstat",         "Sanal Bellek İstatistikleri"},
    /* 15 */ {"Bellek",       "/proc/swaps",          "Swap Alanları"},
    /* 16 */ {"Bellek",       "/proc/buddyinfo",      "Bellek Parçalanması (Buddy)"},
    /* 17 */ {"Disk",         "/proc/diskstats",      "Disk G/Ç İstatistikleri"},
    /* 18 */ {"Disk",         "/proc/partitions",     "Disk Bölümleri"},
    /* 19 */ {"Disk",         "/proc/filesystems",    "Desteklenen Dosya Sistemleri"},
    /* 20 */ {"İşlemci",      "/proc/interrupts",     "Kesmeler (Interrupts)"},
    /* 21 */ {"İşlemci",      "/proc/softirqs",       "Yazılım Kesmeleri"},
    /* 22 */ {"Ağ",           "/proc/net/udp",        "UDP Soketleri"},
    /* 23 */ {"Ağ",           "/proc/net/route",      "Yönlendirme Tablosu"},
    /* 24 */ {"Ağ",           "/proc/net/arp",        "ARP Tablosu"},
    /* 25 */ {"Ağ",           "/proc/net/sockstat",   "Soket Özeti"},
    /* 26 */ {"Ağ",           "/proc/net/snmp",       "Ağ Protokol İstatistikleri"},
    /* 27 */ {"Ağ",           "/proc/net/if_inet6",   "IPv6 Adresleri"},
    /* 28 */ {"Çekirdek",     "/proc/devices",        "Kayıtlı Aygıtlar"},
    /* 29 */ {"Çekirdek",     "/proc/locks",          "Dosya Kilitleri"},
    /* 30 */ {"Baskı (PSI)",  "/proc/pressure/cpu",   "CPU Baskısı"},
    /* 31 */ {"Baskı (PSI)",  "/proc/pressure/memory","Bellek Baskısı"},
    /* 32 */ {"Baskı (PSI)",  "/proc/pressure/io",    "G/Ç Baskısı"},
    /* 33 */ {"Sistem",       "/proc/sys/kernel/hostname",  "Makine Adı"},
    /* 34 */ {"Sistem",       "/proc/sys/kernel/osrelease", "Çekirdek Sürüm Numarası"},
    /* 35 */ {"Sistem",       "/proc/sys/fs/file-nr", "Açık Dosya Sayısı"},
    /* 36 */ {"Sistem",       "/proc/self/limits",    "Sunucu Kaynak Limitleri"},
    /* 37 */ {"Yapılandırma", "/etc/os-release",      "İşletim Sistemi Bilgisi"},
    /* 38 */ {"Yapılandırma", "/etc/hostname",        "Hostname Dosyası"},
    /* 39 */ {"Yapılandırma", "/etc/hosts",           "Hosts Dosyası"},
    /* 40 */ {"Yapılandırma", "/etc/resolv.conf",     "DNS Ayarları"},
    /* 41 */ {"Donanım",      "/sys/class/thermal/thermal_zone0/temp",
                             "CPU Sıcaklığı (milidereceC)"},
    /* 42 */ {"Donanım",      "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
                             "CPU0 Frekansı (kHz)"},
};
#define N_SOURCES ((int)(sizeof(SOURCES) / sizeof(SOURCES[0])))

static const char *GROUPS[] = {
    "Sistem", "İşlemci", "Bellek", "Disk", "Ağ",
    "Çekirdek", "Baskı (PSI)", "Yapılandırma", "Donanım",
};
#define N_GROUPS ((int)(sizeof(GROUPS) / sizeof(GROUPS[0])))

typedef struct { int m; int full; } DashItem;
static const DashItem DASHBOARD[] = {
    {1, 0},  {4, 0},            // yük, çalışma süresi
    {30, 0}, {31, 0},           // CPU / bellek baskısı
    {5, 1},  {10, 1},           // sürüm, komut satırı
    {2, 1},  {17, 1}, {11, 1},  // bellek, disk G/Ç, mount
    {6, 1},  {25, 1},           // ağ trafiği, soket özeti
};
#define N_DASH ((int)(sizeof(DASHBOARD) / sizeof(DASHBOARD[0])))

static const char *CSS =
    "<style>\n"
    "body{font-family:monospace;background:#0f172a;color:#f8fafc;padding:20px;margin:0}\n"
    ".wrap{max-width:1100px;margin:0 auto}\n"
    ".grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}\n"
    ".card{background:#1e293b;padding:20px;border-radius:10px;border:1px solid #334155;margin-bottom:20px}\n"
    ".grid .card{margin-bottom:0}\n"
    ".full{grid-column:span 2}\n"
    "h1{color:#38bdf8;font-size:22px;text-align:center}\n"
    "h2{color:#38bdf8;font-size:15px;margin-top:0;border-bottom:1px solid #334155;padding-bottom:8px}\n"
    "pre{background:#0f172a;padding:12px;border-radius:6px;color:#34d399;overflow:auto;"
    "white-space:pre-wrap;font-size:12px;max-height:400px}\n"
    "a{color:#38bdf8}\n"
    "ul{line-height:2;margin:0;padding-left:20px}\n"
    ".bar{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px}\n"
    "button{background:#334155;color:#f8fafc;border:1px solid #475569;padding:10px 16px;"
    "border-radius:8px;cursor:pointer;font-family:monospace;font-size:13px;font-weight:bold}\n"
    "@media(max-width:700px){.grid{grid-template-columns:1fr}.full{grid-column:span 1}}\n"
    "</style>\n";

// Metin textContent ile yazıldığı için HTML kaçışı gerekmez.
static const char *JS =
    "<script>\n"
    "let auto = true, timer = null;\n"
    "try { auto = localStorage.getItem('autoRefresh') !== 'false'; } catch (e) {}\n"
    "const btn = document.getElementById('toggleBtn');\n"
    "async function load() {\n"
    "  for (const el of document.querySelectorAll('pre[data-m]')) {\n"
    "    try {\n"
    "      const r = await fetch('/data?m=' + el.dataset.m);\n"
    "      el.textContent = await r.text();\n"
    "    } catch (e) { el.textContent = 'Bağlantı hatası'; }\n"
    "  }\n"
    "}\n"
    "function apply() {\n"
    "  btn.textContent = auto ? '⏸ Otomatik yenilemeyi durdur' : '▶ Otomatik yenilemeyi başlat';\n"
    "  clearInterval(timer);\n"
    "  if (auto) timer = setInterval(load, 3000);\n"
    "}\n"
    "btn.onclick = () => {\n"
    "  auto = !auto;\n"
    "  try { localStorage.setItem('autoRefresh', auto); } catch (e) {}\n"
    "  apply();\n"
    "};\n"
    "load(); apply();\n"
    "</script>\n";

// ---------- Yardımcılar ----------

static int send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void send_response(int fd, const char *status, const char *ctype,
                          const char *body, size_t len) {
    char head[256];
    int n = snprintf(head, sizeof(head),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n",
        status, ctype, len);
    if (send_all(fd, head, (size_t)n) == 0 && len > 0)
        send_all(fd, body, len);
}

// Taşmaya karşı güvenli ekleme: sayfa dolarsa kesilir, bellek bozulmaz.
static void app(char *page, size_t cap, size_t *off, const char *fmt, ...) {
    if (*off >= cap - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(page + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    *off += (size_t)n;
    if (*off > cap - 1) *off = cap - 1;
}

// Dosyayı sonuna kadar okur (/proc dosyaları boyut bildirmez). free() gerekir.
static char *read_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) { fclose(fp); return NULL; }

    for (;;) {
        if (len + 1 >= cap) {
            if (cap >= MAX_PROC_READ) break;
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) break;
            buf = tmp;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        if (n == 0) break;
        len += n;
    }
    fclose(fp);
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

// "m=N" değerini çıkarır; geçersizse 0 döner
static int parse_m(const char *path) {
    const char *p = strstr(path, "m=");
    if (!p) return 0;
    int m = atoi(p + 2);
    return (m >= 1 && m <= N_SOURCES) ? m : 0;
}

// ---------- Sayfalar ----------

static void page_begin(char *page, size_t cap, size_t *off, const char *title) {
    app(page, cap, off,
        "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>\n"
        "<title>%s</title>\n%s</head>\n<body><div class='wrap'>\n"
        "<h1>🟢 %s</h1>\n", title, CSS, title);
}

static void page_bar(char *page, size_t cap, size_t *off) {
    app(page, cap, off,
        "<div class='bar'><a href='/'>← Menü</a>"
        "<button id='toggleBtn'>...</button></div>\n");
}

static void page_end(char *page, size_t cap, size_t *off) {
    app(page, cap, off, "</div>\n%s</body></html>\n", JS);
}

static void serve_index(int fd) {
    char page[16384];
    size_t off = 0;
    page_begin(page, sizeof(page), &off, "Sistem İzleyici");
    app(page, sizeof(page), &off,
        "<div class='card'><ul><li><a href='/dashboard'><b>Master Dashboard</b></a></li></ul></div>\n");

    for (int g = 0; g < N_GROUPS; g++) {
        app(page, sizeof(page), &off, "<div class='card'><h2>%s</h2><ul>\n", GROUPS[g]);
        for (int i = 0; i < N_SOURCES; i++) {
            if (strcmp(SOURCES[i].group, GROUPS[g]) != 0) continue;
            app(page, sizeof(page), &off,
                "<li><a href='/view?m=%d'>%s</a> <small>(%s)</small></li>\n",
                i + 1, SOURCES[i].title, SOURCES[i].path);
        }
        app(page, sizeof(page), &off, "</ul></div>\n");
    }
    app(page, sizeof(page), &off, "</div></body></html>\n");
    send_response(fd, "200 OK", "text/html; charset=utf-8", page, off);
}

static void serve_view(int fd, int m) {
    char page[8192];
    size_t off = 0;
    page_begin(page, sizeof(page), &off, SOURCES[m - 1].title);
    page_bar(page, sizeof(page), &off);
    app(page, sizeof(page), &off,
        "<div class='card'><h2>%s</h2><pre data-m='%d'>Yükleniyor...</pre></div>\n",
        SOURCES[m - 1].path, m);
    page_end(page, sizeof(page), &off);
    send_response(fd, "200 OK", "text/html; charset=utf-8", page, off);
}

static void serve_dashboard(int fd) {
    char page[16384];
    size_t off = 0;
    page_begin(page, sizeof(page), &off, "Master Sistem Paneli");
    page_bar(page, sizeof(page), &off);
    app(page, sizeof(page), &off, "<div class='grid'>\n");
    for (int i = 0; i < N_DASH; i++) {
        int m = DASHBOARD[i].m;
        app(page, sizeof(page), &off,
            "<div class='card%s'><h2>%s (%s)</h2><pre data-m='%d'>Yükleniyor...</pre></div>\n",
            DASHBOARD[i].full ? " full" : "", SOURCES[m - 1].title, SOURCES[m - 1].path, m);
    }
    app(page, sizeof(page), &off, "</div>\n");
    page_end(page, sizeof(page), &off);
    send_response(fd, "200 OK", "text/html; charset=utf-8", page, off);
}

static void serve_data(int fd, int m) {
    size_t len = 0;
    char *content = read_file(SOURCES[m - 1].path, &len);
    if (!content) {
        const char *err = "Bu sistemde mevcut değil veya erişilemiyor.";
        send_response(fd, "200 OK", "text/plain; charset=utf-8", err, strlen(err));
        return;
    }
    send_response(fd, "200 OK", "text/plain; charset=utf-8", content, len);
    free(content);
}

static void handle_client(int fd) {
    char req[2048];
    ssize_t n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) return;
    req[n] = '\0';

    char method[8], path[256];
    if (sscanf(req, "%7s %255s", method, path) != 2 || strcmp(method, "GET") != 0) {
        const char *msg = "Yalnızca GET desteklenir.";
        send_response(fd, "405 Method Not Allowed", "text/plain; charset=utf-8", msg, strlen(msg));
        return;
    }

    int m = parse_m(path);

    if (strcmp(path, "/") == 0) {
        serve_index(fd);
    } else if (strcmp(path, "/dashboard") == 0) {
        serve_dashboard(fd);
    } else if (strncmp(path, "/view?", 6) == 0 && m) {
        serve_view(fd, m);
    } else if (strncmp(path, "/data?", 6) == 0 && m) {
        serve_data(fd, m);
    } else {
        const char *msg = "404 - Bulunamadı";
        send_response(fd, "404 Not Found", "text/plain; charset=utf-8", msg, strlen(msg));
    }
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Geçersiz port.\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror("setsockopt");

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // yalnızca 127.0.0.1
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("✓ Sunucu çalışıyor: http://127.0.0.1:%d\n", port);
    fflush(stdout);

    for (;;) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        struct timeval tv = {2, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        handle_client(client);
        close(client);
    }
}
