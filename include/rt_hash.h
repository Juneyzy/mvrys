/*
*   rt_hash.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Message Queue for inter-thread communication
*   Personal.Q
*/


#ifndef __RV_HASH_H__
#define __RV_HASH_H__

#include <stdint.h>

typedef uint32_t HASH_INDEX;

#define HASH_JEN_MIX(a,b,c)                                             \
    do {                                                                            \
        a -= b; a -= c; a ^= ( c >> 13 );                               \
        b -= c; b -= a; b ^= ( a << 8 );                                \
        c -= a; c -= b; c ^= ( b >> 13 );                               \
        a -= b; a -= c; a ^= ( c >> 12 );                               \
        b -= c; b -= a; b ^= ( a << 16 );                               \
        c -= a; c -= b; c ^= ( b >> 5 );                                \
        a -= b; a -= c; a ^= ( c >> 3 );                                \
        b -= c; b -= a; b ^= ( a << 10 );                               \
        c -= a; c -= b; c ^= ( b >> 15 );                               \
    } while (0)

static __rt_always_inline__ uint32_t 
hash_data(void *key, int keylen)
{
    unsigned _hj_i, _hj_j, _hj_k;
    char *_hj_key = (char *) key;
    uint32_t hashv = 0xfeedbeef;
    _hj_i = _hj_j = 0x9e3779b9;
    _hj_k = keylen;

    while (_hj_k >= 12)
    {
        _hj_i += (_hj_key[0] + ((unsigned) _hj_key[1] << 8)
                  + ((unsigned) _hj_key[2] << 16)
                  + ((unsigned) _hj_key[3] << 24));
        _hj_j += (_hj_key[4] + ((unsigned) _hj_key[5] << 8)
                  + ((unsigned) _hj_key[6] << 16)
                  + ((unsigned) _hj_key[7] << 24));
        hashv += (_hj_key[8] + ((unsigned) _hj_key[9] << 8)
                  + ((unsigned) _hj_key[10] << 16)
                  + ((unsigned) _hj_key[11] << 24));
        HASH_JEN_MIX(_hj_i, _hj_j, hashv);
        _hj_key += 12;
        _hj_k -= 12;
    }

    hashv += keylen;

    switch (_hj_k)
    {
        case 11:
            hashv += ((unsigned) _hj_key[10] << 24);

        case 10:
            hashv += ((unsigned) _hj_key[9] << 16);

        case 9:
            hashv += ((unsigned) _hj_key[8] << 8);

        case 8:
            _hj_j += ((unsigned) _hj_key[7] << 24);

        case 7:
            _hj_j += ((unsigned) _hj_key[6] << 16);

        case 6:
            _hj_j += ((unsigned) _hj_key[5] << 8);

        case 5:
            _hj_j += _hj_key[4];

        case 4:
            _hj_i += ((unsigned) _hj_key[3] << 24);

        case 3:
            _hj_i += ((unsigned) _hj_key[2] << 16);

        case 2:
            _hj_i += ((unsigned) _hj_key[1] << 8);

        case 1:
            _hj_i += _hj_key[0];
    }

    HASH_JEN_MIX(_hj_i, _hj_j, hashv);
    return hashv;
}


struct rt_table_bucket
{
	void *value;
	uint16_t size;
	struct rt_table_bucket *next;
};

struct rt_hash_table
{
	/** hash bucket */
	struct rt_table_bucket **array;
	
	/** sizeof hash bucket */
	int array_size;

	/** total of instance stored in bucket, maybe great than array_size in future. */
	int count;

	/** function for create a hash value with the given parameter *v */
	uint32_t (*hash_fn)(struct rt_hash_table *, void *, uint16_t);

	/** 0: equal, otherwise a value less than zero returned*/
	int (*cmp_fn)(void *, uint16_t, void *, uint16_t);

	/** function for vlaue of rt_table_bucket releasing */
	void (*free_fn)(void *);
};

struct rt_hash_table *
rt_hash_table_init(uint32_t size,
		uint32_t (*hash_fn)(struct rt_hash_table *, void *, uint16_t),
		int (*cmp_fn)(void *, uint16_t, void *, uint16_t),
		void (*free_fn)(void *));

void
rt_hash_table_destroy(struct rt_hash_table *ht);

int
rt_hash_table_add(struct rt_hash_table *ht,
		void *v, uint16_t s);


int
rt_hash_table_del(struct rt_hash_table *ht,
		void *v, uint16_t s);


void *
rt_hash_table_lookup(struct rt_hash_table *ht,
		void *v, uint16_t s);

#ifdef __cplusplus
}
#endif

#endif  /* __RV_HASH_H__ */

