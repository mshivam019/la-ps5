#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *__real_getcwd(char *, size_t);
int main(void) {
    char host[PATH_MAX], after[PATH_MAX], root[] = "/tmp/la-paths-XXXXXX";
    assert(__real_getcwd(host, sizeof host));
    assert(mkdtemp(root));
    assert(chdir(root) == 0);
    assert(__real_getcwd(after, sizeof after) && strcmp(host, after) == 0);
    char *cwd = getcwd(NULL, 0);
    assert(cwd && !strcmp(cwd, root));
    free(cwd);
    assert(!getcwd(after, 1) && errno == ERANGE);
    assert(mkdir("save", 0777) == 0);
    FILE *f = fopen("save/slot", "wb");
    assert(f && fwrite("saved", 1, 5, f) == 5 && fclose(f) == 0);
    int fd = open("save/slot", O_RDONLY);
    char value[6] = {0};
    assert(fd >= 0 && read(fd, value, 5) == 5 && !strcmp(value, "saved"));
    close(fd);
    struct stat st;
    assert(stat("save/slot", &st) == 0 && st.st_size == 5);
    assert(lstat("save/slot", &st) == 0 && access("save/slot", R_OK) == 0);
    assert(realpath("save/../save/slot", after));
    assert(strstr(after, "/save/slot"));
    assert(rename("save/slot", "save/renamed") == 0);
    DIR *dir = opendir("save");
    assert(dir);
    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(dir))) found |= !strcmp(entry->d_name, "renamed");
    assert(found && closedir(dir) == 0);
    assert(chdir("save/renamed") == -1 && errno == ENOTDIR);
    assert(chdir("save") == 0 && unlink("renamed") == 0);
    assert(chdir("..") == 0 && rmdir("save") == 0);
    fd = open("temporary", O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0 && close(fd) == 0 && remove("temporary") == 0);
    assert(chdir(host) == 0 && rmdir(root) == 0);
    puts("PASS: virtual cwd, file IO, saves, rename, directory iteration, path errors");
}
