#ifndef __YAML_IO_H__
#define __YAML_IO_H__

int 
ConfYamlReadInt(char *, int *);
int 
ConfYamlReadStr(char *, char *, int);
void
ConfYamlDestroy();
void
ConfYamlLoadFromFile(const char *file);

#endif
