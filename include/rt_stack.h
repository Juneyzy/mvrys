/*
*   rt_stack.h
*   Created by Tsihang <qihang@semptian.com>
*   25 Mar, 2016
*   Func: Stack Component
*   Personal.Q
*/

#ifndef __RT_STACK_H__
#define __RT_STACK_H__

struct stack_t {

#define STACK_INIT_SIZE 5

    size_t max;
    size_t sp;
    size_t size;
    size_t iter;
    void **array;

    void (*free_fn)(void *);
    
};

extern struct stack_t* rt_stack_new(size_t max, 
                    void (*free_fn)(void *));

extern int rt_stack_push(struct stack_t* _this, void *data);
extern void* rt_stack_pop(struct stack_t* _this);
extern void rt_stack_delete(struct stack_t* _this);

#endif  /** END OF __RT_STACK_H__ */

