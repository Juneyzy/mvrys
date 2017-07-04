#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>
#include <syslog.h>

#define  BUF_SIZE 256

#define likely(expr) __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)

/**Lib C*/
int is_empty_dir (const char *realpath)
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

int is_empty_file(const char *realpath_file)
{
    return (!access(realpath_file, F_OK));
}

void do_system(const char *cmd)
{
    int xret = -1;

    if (likely(cmd)) {
	    xret = system(cmd);
	    if (likely(xret < 0)){
	        ;
	    }
    }
}
/**Lib C*/
void get_name_bypid(pid_t pid, char *task_name) {
    char proc_pid_path[BUF_SIZE];
    char buf[BUF_SIZE];

    sprintf(proc_pid_path, "/proc/%d/status", pid);
    FILE* fp = fopen(proc_pid_path, "r");
    if(NULL != fp){
        if( fgets(buf, BUF_SIZE - 1, fp)== NULL ){
            fclose(fp);
        }
        fclose(fp);
        sscanf(buf, "%*s %s", task_name);
    }
}

/*1 exsit **/
int get_file_type(char *filename, char *file_type)
{
    int xret = -1;
    struct stat buf;
    char *type = NULL;

    stat(filename, &buf );
    if(S_IFDIR & buf.st_mode){
        type = "folder";
        xret = is_empty_dir(filename);
        goto finish;
    }else if(S_IFREG & buf.st_mode){
        type = "file";
        xret = is_empty_file(filename);
        goto finish;
    }
    type = "uknown";

finish:
    snprintf(file_type, 7, "%s", type);
    return xret;
}

static struct option const long_opts[] =
{
  {"directory",    0, 0, 'd'},
  {"force",        0, 0, 'f'},
  {"interactive",  0, 0, 'i'},
  {"recursive",    0, 0, 'r'},
  {NULL,           0, 0, 0}
};

int main(int argc, char *argv[])
{
    size_t i = 0;
    char cmd_line[BUF_SIZE] = {0}, proc_name[BUF_SIZE] = {0};
    char file_type[8] = {0};
    int xret = -1;
    char opt = '\0';

    while ((opt = getopt_long (argc, argv, "dfir", long_opts, NULL)) != -1);
    if (optind == argc){
        goto finish;
    }

    size_t n_files = argc - optind;
    char **file = (char **) argv + optind;

    for (i = 0; i < n_files; i ++){
        snprintf(cmd_line, BUF_SIZE -1, "rm %s -rf", file[i]);
        xret = get_file_type(file[i], file_type);
        if (xret > 0){
            get_name_bypid(getppid(), proc_name);
            do_system(cmd_line);

            syslog(LOG_INFO, "pid = %d, ProcessName = %s, delete %s = %s", getppid(),
                   proc_name,  file_type, file[i]);
        }
    }

finish:
    return 0;
}
