#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>


#include "rt_errno.h"
#include "rt_common.h"

#define FPUTS(str, fp) fputs((const char *)str, fp)


int rt_file_exsit(const char *realpath_file)
{
    return (!access(realpath_file, F_OK));
}

int rt_dir_exsit (const char *realpath)
{
    DIR *dir = NULL;
    int exsit = 0;

    if (unlikely (!realpath))
		goto finish;
	
    dir = opendir(realpath);
    if (!dir) goto finish;
    
    exsit = 1;
    closedir(dir);
finish:
    return exsit;
}

int rt_file_open(const char *realpath_file, 
    FILE **fp)
{
    declare_array(char, fmode, 8);
    
    assert(realpath_file);
    
    if(rt_file_exsit(realpath_file)){
        strcpy(fmode, "a+");
    }
    else
        strcpy(fmode, "w+");
        
    *fp = fopen(realpath_file, fmode);
    if (!*fp){
        printf ("fopen: \"%s\", \"%s\", %s\n", realpath_file, fmode, strerror(errno));
    }

    return 0;
}


int rt_file_write(FILE *fp, 
    void *val, 
    size_t __attribute__((__unused__))s,
    size_t __attribute__((__unused__))(*format_fn)(void *, char *, size_t))
{
    return FPUTS((const char *)val, fp);
}

int rt_dir_lookup(const char *realpath, const char *f)
{
    int xret = (-ERRNO_NO_ELEMENT);

    struct dirent *entry;
    struct stat statbuf;    
    
    DIR *dir = opendir(realpath);
    assert(dir);

    while ((entry = readdir(dir)) != NULL) {
    	lstat(entry->d_name, &statbuf);
    	if (S_ISDIR(statbuf.st_mode)) {
    		if (strcmp(entry->d_name, ".") == 0 || 
    			strcmp(entry->d_name, "..") == 0 )  
    			continue;
            /** do recursion */
    	} 
        else{
                /** printf("d_name=\"%s\", f=\"%s\"\n", entry->d_name, f);*/
                if (!strcmp(entry->d_name, f)){
                    xret = XSUCCESS;
                    goto finish;
                }
       }
    }
finish:
    closedir(dir);  
    return xret;
}

int 
rt_dir_print(char *realpath, 
    int depth)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    
    if ((dir = opendir(realpath)) == NULL) {
    	/** fprintf(stderr, "Can`t open directory %s\n", dir); */
    	return (-ERRNO_ACCESS_DENIED);
    }
    while ((entry = readdir(dir)) != NULL) {
    	lstat(entry->d_name, &statbuf);
    	if (S_ISDIR(statbuf.st_mode)) {
    		if (strcmp(entry->d_name, ".") == 0 || 
    			strcmp(entry->d_name, "..") == 0 )  
    			continue;
            /** do recursion */
    	} 
        else{
                printf("%*s%s\n", depth, "", entry->d_name);
       }
    }
    closedir(dir);	

    return XSUCCESS;
}

int rt_remove_dir(const char *root)
{
    char cmd[256] = {0};

    if(rt_dir_exsit(root)) {
        sprintf(cmd, "rm -rf %s", root);
        system(cmd);
        return 1;
    }

    return 0;
}

int rt_check_and_mkdir(const char *root)
{
    char cmd[256] = {0};

    if(!rt_dir_exsit(root)) {
        sprintf(cmd, "mkdir -p %s", root);
        system(cmd);
        return 1;
    }

    return 0;
}


