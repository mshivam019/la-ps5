// A sandboxed PS5 title cannot chdir. Resolve relative paths at the application
// boundary. The native default keeps prepared data and user saves under /app0.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char cwd[PATH_MAX] = "/app0";
static pthread_mutex_t cwd_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *absolute(const char *path, char out[PATH_MAX]) {
    if (!path || !*path) { errno = ENOENT; return NULL; }
    if (*path == '/') return path;
    pthread_mutex_lock(&cwd_lock);
    int length = snprintf(out, PATH_MAX, "%s/%s", cwd, path);
    pthread_mutex_unlock(&cwd_lock);
    if (length < 0 || length >= PATH_MAX) { errno = ENAMETOOLONG; return NULL; }
    return out;
}

int __real_stat(const char *, struct stat *);
int __real_lstat(const char *, struct stat *);
int __real_access(const char *, int);
int __real_mkdir(const char *, mode_t);
int __real_rmdir(const char *);
int __real_unlink(const char *);
int __real_remove(const char *);
int __real_rename(const char *, const char *);
int __real_open(const char *, int, ...);
FILE *__real_fopen(const char *, const char *);
DIR *__real_opendir(const char *);
#ifdef __PROSPERO__
#include <ps5platform/libc.h>
#define native_realpath ps5_realpath
#else
char *__real_realpath(const char *, char *);
#define native_realpath __real_realpath
#endif

#define ONE_PATH(name, extra_decl, extra_arg) \
    int __wrap_##name(const char *path extra_decl) { \
        char buffer[PATH_MAX]; const char *p = absolute(path, buffer); \
        return p ? __real_##name(p extra_arg) : -1; \
    }
#define COMMA ,
ONE_PATH(stat, COMMA struct stat *s, COMMA s)
ONE_PATH(lstat, COMMA struct stat *s, COMMA s)
ONE_PATH(access, COMMA int mode, COMMA mode)
ONE_PATH(mkdir, COMMA mode_t mode, COMMA mode)
ONE_PATH(rmdir, , )
ONE_PATH(unlink, , )
ONE_PATH(remove, , )

int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    char buffer[PATH_MAX]; const char *p = absolute(path, buffer);
    return p ? __real_open(p, flags, mode) : -1;
}
FILE *__wrap_fopen(const char *path, const char *mode) {
    char buffer[PATH_MAX]; const char *p = absolute(path, buffer);
    return p ? __real_fopen(p, mode) : NULL;
}
DIR *__wrap_opendir(const char *path) {
    char buffer[PATH_MAX]; const char *p = absolute(path, buffer);
    return p ? __real_opendir(p) : NULL;
}
char *__wrap_realpath(const char *path, char *resolved) {
    char buffer[PATH_MAX]; const char *p = absolute(path, buffer);
    return p ? native_realpath(p, resolved) : NULL;
}
int __wrap_rename(const char *from, const char *to) {
    char a[PATH_MAX], b[PATH_MAX];
    const char *pa = absolute(from, a), *pb = absolute(to, b);
    return pa && pb ? __real_rename(pa, pb) : -1;
}
char *__wrap_getcwd(char *out, size_t size) {
    pthread_mutex_lock(&cwd_lock);
    size_t length = strlen(cwd) + 1;
    bool allocated = !out;
    if (!out) { if (!size) size = length; out = malloc(size); }
    if (!out || size < length) {
        if (allocated) free(out);
        pthread_mutex_unlock(&cwd_lock); errno = size < length ? ERANGE : ENOMEM; return NULL;
    }
    memcpy(out, cwd, length);
    pthread_mutex_unlock(&cwd_lock);
    return out;
}
int __wrap_chdir(const char *path) {
    char buffer[PATH_MAX], resolved[PATH_MAX];
    const char *p = absolute(path, buffer);
    struct stat st;
    if (!p || !native_realpath(p, resolved) || __real_stat(resolved, &st)) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    pthread_mutex_lock(&cwd_lock);
    strcpy(cwd, resolved);
    pthread_mutex_unlock(&cwd_lock);
    return 0;
}
