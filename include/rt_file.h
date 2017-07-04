#ifndef __UTIL_FILE_H__
#define __UTIL_FILE_H__

extern int rt_file_open(const char *realpath_file, FILE **fp);
extern int rt_file_exsit(const char *);
extern int rt_dir_exsit (const char *realpath);
extern int rt_dir_lookup(const char *, const char *);
extern int rt_remove_dir(const char *root);
extern int rt_check_and_mkdir(const char *root);

extern void rt_mem_alloc(void **ptr, size_t s);
extern int rt_file_write(FILE *fp, 
    void *val, 
    size_t __attribute__((__unused__))s,
    size_t __attribute__((__unused__))(*format_fn)(void *, char *, size_t));

#endif
