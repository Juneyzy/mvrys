
#include "rt_string.h"
#include "rt_util.h"
#include "conf.h"
#include "conf-yaml-loader.h"

/* read integer configure from xxx.ymal */
int 
ConfYamlReadInt(char *section, int *value)
{
    char *pc_val;

    if (!ConfGet(section, &pc_val))
        goto finish;
    
    if ((pc_val == NULL) || (pc_val[0] == 0))
        goto finish;

    if (isalldigit(pc_val))
    *value = atoi(pc_val);

    return 0;
    
finish:
    return -1;
}

/* read string configure from xxx.ymal */
int 
ConfYamlReadStr(char *section, char *pc_value, int size)
{
    char *pc_val;
    int len;
    int length = size;

    if (!ConfGet(section, &pc_val))
        goto finish;
    if ((pc_val == NULL) || (pc_val[0] == 0))
        goto finish;
    
    len = strlen(pc_val);
    if (len < length)
    {
        strcpy(pc_value, pc_val);
    }
    return 0;   
finish:
    return -1;
}

void ConfYamlDestroy()
{
    ConfDeInit();
}

void
ConfYamlLoadFromFile(const char *file)
{
    ConfInit();
    if (likely(file))
	    ConfYamlLoadFile(file);
}
