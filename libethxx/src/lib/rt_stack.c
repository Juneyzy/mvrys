/*
*   rt_stack.c
*   Created by Tsihang <qihang@semptian.com>
*   25 Mar, 2016
*   Func: Stack Component
*   Personal.Q
*/

#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
#include "rt_common.h"
#include "rt_stdlib.h"
#include "rt_stack.h"

/******************************************************************************/
size_t rt_stack_get_nelem(const struct stack_t* _this)
{
    return _this ? _this->sp : (size_t)-1;
}

/******************************************************************************/
void rt_stack_delete(struct stack_t* _this)
{
    if (!_this)
	return;

    if (_this->free_fn) {
        while (_this->sp > 0) {
            _this->free_fn(_this->array[--(_this->sp)]);
        }
    }
    
    kfree(_this->array);
    kfree(_this);
}

/******************************************************************************/
void* rt_stack_begin(struct stack_t* _this)
{
    if (!_this)
        return NULL;

    _this->iter = 0;
    return _this->array[_this->iter];
}

/******************************************************************************/
void* rt_stack_next(struct stack_t* _this)
{
    if (_this && _this->iter < _this->sp)
        return _this->array[_this->iter++];

	return NULL;
}

/******************************************************************************/
void* rt_stack_peek(struct stack_t* _this)
{
    if (!_this || !_this->sp)
        return NULL;

    return _this->array[_this->sp - 1];
}

/******************************************************************************/
void* rt_stack_end(struct stack_t* _this)
{
    return rt_stack_peek(_this);
}


/******************************************************************************/
struct stack_t* rt_stack_new(size_t max, 
                    void (*free_fn)(void *))
{
    struct stack_t* _this;

    _this        = (struct stack_t*)kcalloc(1, sizeof(struct stack_t), MPF_CLR, -1);
    _this->max   = (max == 0) ? INT_MAX : max;
    _this->size  = STACK_INIT_SIZE;
    _this->sp    = 0;
    _this->array = (void **)kcalloc(_this->size, sizeof(*_this->array), MPF_CLR, -1);
    _this->free_fn = free_fn;

    return _this;
}


/******************************************************************************/
int rt_stack_push(struct stack_t* _this, void *data)
{
    if (_this == NULL)
        return -1;

    if (_this->sp == _this->size) {

        size_t new_size;

        if (_this->size == _this->max)
            return -1;

        if (_this->size * 2 > _this->max) 
            new_size = _this->max;
        else
            new_size = _this->size * 2;
    
        _this->size = new_size;
        printf("resize the stack\n");
        _this->array = (void **)krealloc(_this->array, sizeof(*_this->array) * _this->size, MPF_NOFLGS, -1);
    }

    assert(_this->sp <= _this->size);

    _this->array[_this->sp++] = data;

    return 0;
}

/******************************************************************************/
void* rt_stack_pop(struct stack_t* _this)
{
    if (_this == NULL 
        || _this->sp == 0)
	return NULL;

    if (_this->size >= STACK_INIT_SIZE * 4 
        && _this->sp < _this->size / 4) {
        size_t new_size = _this->size / 2;
        _this->size = new_size;
        _this->array = (void **)krealloc(_this->array, sizeof(*_this->array) * _this->size, MPF_NOFLGS, -1);
    }

    assert(_this->sp > 0 && _this->sp <= _this->size);
    return _this->array[--(_this->sp)];
}

