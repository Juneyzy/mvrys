#ifndef __UTIL_ENUM_H__
#define __UTIL_ENUM_H__

typedef struct {
    const char *enum_name;
    int enum_value;
} rt_enum_val_map;

int rt_enum_n2v(const char *, rt_enum_val_map *);

const char * rt_enum_v2n(int, rt_enum_val_map *);

#endif /* __UTIL_ENUM_H__ */

