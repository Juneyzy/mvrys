/*
*   serv.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The service registry interface for lwrpc
*   Personal.Q
*/

#include "sysdefs.h"
#include "serv.h"

typedef struct
{
    int valid;
    int svcnt;              /* How many service instance */
    struct rb_root root;    /* Root of rbtree */
} serv_pool;


static pthread_mutex_t serv_key;

#define ENTER_SERV_CRITICAL()  pthread_mutex_lock(&serv_key)
#define EXIT_SERV_CRITICAL()   pthread_mutex_unlock(&serv_key)

static serv_pool *g_ix_serv_management;

static inline int serv_id_cmp(const struct rb_node *node, const void *ptr)
{
    serv_element *em = rb_entry(node, serv_element, rb);
    /* printf ("%d, key=%d\n", em->serv_id, (unsigned short int)(intptr_t)ptr);*/
    return em->serv_id - (unsigned short int)(intptr_t)ptr;
}

static inline serv_element *serv_element_init(
    unsigned short int serv_id,
    int (*srvFunc)(unsigned char *, unsigned char *, int *, int *)
)
{
    serv_element *svc_elem;
    svc_elem = __TCalloc(1, serv_element);
    svc_elem->serv_id = serv_id;
    svc_elem->srvFunc = srvFunc;
    return svc_elem;
}

void serv_init(void)
{
    serv_pool *svc;

    if (g_ix_serv_management && g_ix_serv_management->valid)
    { return; }

    pthread_mutex_init(&serv_key, NULL);
    svc = __TCalloc(1, serv_pool);
    svc->svcnt = 0;
    svc->valid = 1;
    rb_init(&svc ->root, serv_id_cmp);
    g_ix_serv_management = svc;
    return;
}

int serv_register(
    unsigned short int serv_id,
    int (*srvFunc)(unsigned char *, unsigned char *, int *, int *)
)
{
    serv_element *svc_elem;
    struct rb_node *node;
    node = rb_find(&g_ix_serv_management->root, (void *)(intptr_t)serv_id);

    if (node)
    {
        /* Have alreay registerred, can not register again */
        printf("Error: [%d] has already been registerred !\n", serv_id);
        return -1;
    }

    svc_elem = serv_element_init(serv_id, srvFunc);
    ENTER_SERV_CRITICAL();
    rb_insert(&svc_elem ->rb,
              &g_ix_serv_management ->root,
              (const void *)(intptr_t)svc_elem->serv_id
             );
    g_ix_serv_management ->svcnt ++;
    EXIT_SERV_CRITICAL();
    return 0;
}

/* */
int serv_unregister(unsigned short int serv_id)
{
    serv_element *em;
    struct rb_node *node;
    /* find the rb node, specified by lwrpc id */
    node = rb_find(&g_ix_serv_management ->root, (void *)(intptr_t)serv_id);

    /* Locate to the start address where the node stored in the structure */
    if (node)
    {
        em = rb_entry(node, serv_element, rb);

        if (em)
        {
            printf("Unregister [%d] ... \n", serv_id);
            ENTER_SERV_CRITICAL();
            rb_erase(&em->rb, &g_ix_serv_management ->root);
            EXIT_SERV_CRITICAL();
        }
    }
    else
    {
        printf("Error: [%d] no such instance !\n", serv_id);
        return -1;
    }

    ENTER_SERV_CRITICAL();
    g_ix_serv_management ->svcnt --;
    EXIT_SERV_CRITICAL();
    return 0;
}

serv_element *serv_find(unsigned short int serv_id)
{
    serv_element *element = NULL;
    struct rb_node *node;
    /* find the rb node, specified by lwrpc id */
    node = rb_find(&g_ix_serv_management->root, (void *)(intptr_t)serv_id);

    /* Locate to the start address where the node stored in the structure */
    if (node)
    {
        element = rb_entry(node, serv_element, rb);
    }

    return element;
}

int serv_run(unsigned short int serv_id,
             unsigned char *request,
             unsigned char *result,
             int *return_param_num,
             int *return_bytes
            )
{
    serv_element *element;
    struct rb_node *node;
    /* find the rb node, specified by lwrpc id */
    node = rb_find(&g_ix_serv_management->root, (void *)(intptr_t)serv_id);

    /* Locate to the start address where the node stored in the structure */
    if (node)
    {
        element = rb_entry(node, serv_element, rb);

        if (element->srvFunc && element->serv_id == serv_id)
        {
            /* execute func */
            if (0 != element->srvFunc(request, result, return_param_num, return_bytes))
            {
                printf("Execute -1: rpc id [%d]\n", serv_id);
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        printf("Can not find relation node !\n");
        return -1;
    }

    return 0;
}

int serv_destroy()
{
    return 0;
}

int serv_count(void)
{
    return (g_ix_serv_management->svcnt);
}

