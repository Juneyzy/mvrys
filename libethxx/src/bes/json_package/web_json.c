/*
*   web_json.c
*   Created by Tsihang <qihang@semptian.com>
*   27 Aug, 2015
*   Func: Data Switch Interface between WEB & DECODER
*   Personal.Q
*/

#include "sysdefs.h"
#include "web_json.h"

static inline int sort_fn (const void *j1, const void *j2)
{
	json_object * const *jso1, * const *jso2;
	int i1, i2;

	jso1 = (json_object* const*)j1;
	jso2 = (json_object* const*)j2;
	if (!*jso1 && !*jso2)
		return 0;
	if (!*jso1)
		return -1;
	if (!*jso2)
		return 1;

	i1 = json_object_get_int(*jso1);
	i2 = json_object_get_int(*jso2);

	return i1 - i2;
}

