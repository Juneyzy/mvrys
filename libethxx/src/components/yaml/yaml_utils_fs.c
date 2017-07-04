#include "yaml_common.h"

/**
 *  \brief Check if a path is absolute
 *
 *  \param path string with the path
 *
 *  \retval 1 absolute
 *  \retval 0 not absolute
 */
int PathIsAbsolute(const char *path) {
    if (strlen(path) > 1 && path[0] == '/') {
        return 1;
    }

#if (defined OS_WIN32 || defined __CYGWIN__)
    if (strlen(path) > 2) {
        if (isalpha((unsigned char)path[0]) && path[1] == ':') {
            return 1;
        }
    }
#endif

    return 0;
}

/**
 *  \brief Check if a path is relative
 *
 *  \param path string with the path
 *
 *  \retval 1 relative
 *  \retval 0 not relative
 */
int PathIsRelative(const char *path) {
    return PathIsAbsolute(path) ? 0 : 1;
}

