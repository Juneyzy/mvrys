/*
*   rt_hash.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Message Queue for inter-thread communication
*   Personal.Q
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rt_common.h"
#include "rt_stdlib.h"
#include "rt_hash.h"

static uint32_t
ht_generic_hval_func(struct rt_hash_table *ht,
		void *v, uint16_t s) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint32_t hv = 0;

     for (i = 0; i < s; i++) {
         if (i == 0)      hv += (((uint32_t)*d++));
         else if (i == 1) hv += (((uint32_t)*d++) * s);
         else             hv *= (((uint32_t)*d++) * i) + s + i;
     }

     hv *= s;
     hv %= ht->array_size;
     
     return hv;
}

static int
ht_generic_cmp_func(void *v1, 
		uint16_t s1,
		void *v2,
		uint16_t s2)
{
	int xret = 0;

	if (s1 != s2 ||
		memcmp(v1, v2, s2))
		xret = -1;
	
	return xret;
}

struct rt_hash_table *
rt_hash_table_init(uint32_t size,
		uint32_t (*hash_fn)(struct rt_hash_table *, void *, uint16_t),
		int (*cmp_fn)(void *, uint16_t, void *, uint16_t),
		void (*free_fn)(void *))
{
	struct rt_hash_table *ht = NULL;

	if (size == 0)
		goto finish;

       rt_kmalloc((void **)&ht, sizeof (struct rt_hash_table));

	ht->array_size = size;
	ht->hash_fn = hash_fn ? hash_fn : ht_generic_hval_func;
	ht->free_fn = free_fn;

	if (cmp_fn)
		ht->cmp_fn = cmp_fn;
	else
		ht->cmp_fn = ht_generic_cmp_func;

       rt_kmalloc((void **)&ht->array, ht->array_size * sizeof (struct rt_table_bucket *));
	//ht->array = malloc (ht->array_size * sizeof (struct rt_table_bucket *));
	if (!ht->array)
		goto error;

       goto finish;
       
error:
	if (ht){
		if (ht->array)
			free (ht->array);
		free (ht);
	}

finish:
	return ht;
}

void
rt_hash_table_destroy(struct rt_hash_table *ht)
{
	struct rt_table_bucket *tb, *next = NULL;
	int hv = 0;

	if (!ht)
		return;
	
	for (hv = 0; hv < ht->array_size; hv ++){
		tb = ht->array[hv];
		while (tb){
			next = tb->next;
			if (ht->free_fn)
				ht->free_fn(tb->value);
			free(tb);
			tb = next;
		}
	}

	if (ht->array)
		free (ht->array);
	free (ht);
	return;
}

int
rt_hash_table_add(struct rt_hash_table *ht,
		void *v, uint16_t s)
{
	uint32_t hv = 0;
	int xret = -1;
	struct rt_table_bucket *tb;

	if (!ht ||
		!v || !s)
		goto finish;

	hv = ht->hash_fn(ht, v, s);
	tb = malloc (sizeof (struct rt_table_bucket));
	if (!tb)
		goto finish;

	memset (tb, 0, sizeof (struct rt_table_bucket));
	tb->value = v;
	tb->size = s;
	tb->next = NULL;

       
	if (!ht->array[hv]){
		ht->array[hv] = tb;	
	}else{
		tb->next = ht->array[hv];
		ht->array[hv] = tb;
	}
printf("hv=%u\n", hv);
	ht->count ++;
	xret = 0;

finish:
	return xret;
}

int
rt_hash_table_del(struct rt_hash_table *ht,
		void *v, uint16_t s
/** Delete a val: 0 success, a retval less than zero returned if failure*/)
{
	uint32_t hv = 0;
	int xret = -1;

	if (!ht ||
		!v || !s)
		goto finish;

	hv = ht->hash_fn(ht, v, s);
	if (!ht->array[hv])
		goto finish;

	/** 1st, only one bucket  */
	if (!ht->array[hv]->next)
	{
		if (ht->free_fn)
			ht->free_fn(ht->array[hv]->value);
		free(ht->array[hv]);
		ht->array[hv] = NULL;
	}

	struct rt_table_bucket *tb = ht->array[hv], *prev = NULL;
	do {
		if (ht->cmp_fn){
			if (!ht->cmp_fn(tb->value, tb->size, v, s)){
				if (!prev)/** as root */
					ht->array[hv] = tb->next;
				else
					prev->next = tb->next;

				if (!ht->free_fn)
					ht->free_fn(tb->value);
				free(tb);
				xret = 0;
				goto finish;
			}

			prev = tb;
			tb = tb->next;
		}
	}while (tb);

finish:
	return xret;
}

void *
rt_hash_table_lookup(struct rt_hash_table *ht,
		void *v, uint16_t s)
{
	uint32_t hv;
	void *xret = NULL;

	if (!ht ||
		!v || !s)
		goto finish;

	hv = ht->hash_fn(ht, v, s);
	if (!ht->array[hv])
		goto finish;

	struct rt_table_bucket *tb = ht->array[hv];
	do{
		if (ht->cmp_fn){
			if (!ht->cmp_fn(tb->value, tb->size, v, s)){
				xret = tb->value;
				goto finish;
			}
		}
		tb = tb->next;
	}while(tb);

finish:
	return xret;
}

int
rt_hash_table_test (void) 
{
    int result = 0;
    struct rt_hash_table *ht = rt_hash_table_init(32, ht_generic_hval_func, NULL, NULL);
    if (ht == NULL)
        goto end;
    printf ("__func__=%s, __line__=%d\n", __FUNCTION__, __LINE__);

	char *val = strdup("test");
    int r = rt_hash_table_add(ht, val, strlen(val));
    if (r != 0)
        goto end;

    printf ("__func__=%s, __line__=%d\n", __FUNCTION__, __LINE__);
    char *rp = rt_hash_table_lookup(ht, "test", 4);
    if (rp == NULL)
        goto end;

printf ("__func__=%s, __line__=%d, rp = %s\n", __FUNCTION__, __LINE__, rp);
    r = rt_hash_table_del(ht, "test2", 5);
    if (r == 0)
        goto end;

printf ("__func__=%s, __line__=%d, r=%d\n", __FUNCTION__, __LINE__, r);
    /* all is good! */
    result = 1;
end:
    rt_hash_table_destroy(ht);
    return result;
}

