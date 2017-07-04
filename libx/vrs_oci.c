#include "sysdefs.h"
#include "ocilib.h"
#include "vrs_oci.h"
#include "vrs.h"

atomic_t oci_init = ATOMIC_INIT(0);


OCI_Statement *oci_state = NULL;
OCI_Connection *oci_connection = NULL;

#define err_handler(oci_err)\
    do {\
        rt_log_notice("OCI error : errcode %d, errmsg %s)", OCI_ErrorGetOCICode(oci_err), OCI_ErrorGetString(oci_err));\
        rt_log_notice("Going to clean up this connection and retry init now");\
        if (oci_state) {\
            OCI_StatementFree(oci_state);\
            oci_state = NULL;\
        }\
        if (oci_connection){\
            OCI_ConnectionFree(oci_connection);\
            oci_connection = NULL;\
        }\
        if (atomic_read(&oci_init)){\
            OCI_Cleanup();\
            atomic_set(&oci_init, 0);\
        }\
    } while (0)\


int vrs_insert_stat_info(struct counter_upload_t *_this)
{
    OCI_Statement *st = oci_state;
    OCI_Connection *cn = oci_connection;
    char sql[512] = {0};
    int xerror = -1;

    if (st && cn)
    {
        snprintf(sql, 512, "insert into hitcount (id,time,hitsession,totalsession,vpwid) values(seq_hitcount.nextval, \'%s\', %d, %d, %d)",
                  _this->tm, _this->hitted_sum, _this->matched_sum, _this->vpw_id);
        rt_log_debug ("go to do sql[%s]", sql);
        //执行sql语句
        if (!OCI_ExecuteStmt(st, sql))
        {
                err_handler (OCI_GetLastError());
                goto finish;
        }
        
        //提交
        if (!OCI_Commit(cn))
        {
                err_handler (OCI_GetLastError());
                goto finish;
        }
        xerror = 0;
    }

finish:
    return xerror;
}

int vrs_get_topx_records(int topx, int clue_id, int object_id, int *score_records)
{
    OCI_Statement *st = oci_state;
    OCI_Connection *cn = oci_connection;
    OCI_Resultset *rs = NULL;
    char sql[2048] = {0};
    int count = 0;

    if (st && cn)
    {
        snprintf(sql, 2048, "select score1 from (select rownum,score1 from (select score1 from T%dCALL where clue_id=%d and capture_time>=(select MAX(capture_time) AS maxtime FROM t%dcall WHERE clue_id=%d)-86400 and capture_time<=(select max(capture_time) as maxtime from t%dcall where clue_id=%d) order by score1 desc)) where rownum<=%d",
            object_id, clue_id, object_id, clue_id, object_id, clue_id, topx);
        rt_log_debug("go to do sql [%s]", sql);
        if (!OCI_Prepare (st, sql)) {
            err_handler (OCI_GetLastError());
            goto finish;
        }
        
        if(!OCI_Execute(st)) {
            err_handler (OCI_GetLastError());
            goto finish;
        }
        
        rs = OCI_GetResultset(st);
        if (!rs) {
            err_handler (OCI_GetLastError());
            goto finish;
        }

        while (OCI_FetchNext (rs)) {
            score_records[count]    =   OCI_GetInt(rs, 1);
            count ++;
        }
        if (rs) {
            OCI_ReleaseResultsets (st);
        }
    }
finish:

    return count;
}


/** 从数据库中查询历史命中结果，存放在链表中，同时返回历史命中的总数 add by yuansheng*/
int vrs_get_hit_records(int clue_id, int object_id, struct hit_records_t *records)
{
    OCI_Statement *st = oci_state;
    OCI_Connection *cn = oci_connection;
    OCI_Resultset *rs = NULL;
    struct hit_records_t *__this;
    char sql[256] = {0};
    int count = 0;

    if (st && cn)
    {
        snprintf(sql, 256, "select c2,callid from T%dCALL where clue_id=%d", object_id, clue_id);
        if (!OCI_Prepare (st, sql)) {
            err_handler (OCI_GetLastError());
            goto finish;
        }
        
        if(!OCI_Execute(st)) {
            err_handler (OCI_GetLastError());
            goto finish;
        }
        
        rs = OCI_GetResultset(st);
        if (!rs) {
            err_handler (OCI_GetLastError());
            goto finish;
        }

        while (OCI_FetchNext (rs)) {
            __this = (struct hit_records_t *)kmalloc(sizeof(struct hit_records_t), MPF_CLR, -1);
            __this->dir    =   OCI_GetInt(rs, 1);
            __this->callid =   OCI_GetUnsignedBigInt(rs, 2);
            __this->next = records->next;
            records->next = __this;
            count ++;
        }
        if (rs) {
            OCI_ReleaseResultsets (st);
        }
    }
finish:

    return count;
}


void vrs_oci_conn_tmr (uint32_t __attribute__((__unused__))uid, int __attribute__((__unused__))argc,
                char __attribute__((__unused__))**argv)
{
    struct vrs_db_info *db = (struct vrs_db_info *)argv;

    if (atomic_read(&oci_init) == 0) {
        if (OCI_Initialize (NULL, NULL, OCI_ENV_DEFAULT | OCI_ENV_THREADED | OCI_ENV_CONTEXT)) {
            atomic_inc(&oci_init);
            rt_log_info ("Compile version: %d", OCI_GetOCICompileVersion());
            rt_log_info ("Runtime version: %d", OCI_GetOCIRuntimeVersion());
        }
        else {
            rt_log_error (ERRNO_FATAL, "OCI init fail!");
        }
    }

    if (atomic_read(&oci_init)) {
        if (!oci_connection) {
            rt_log_info ("Try connecting to CaseDB ...");
            oci_connection = OCI_ConnectionCreate(db->dbname, db->usrname, db->passwd, OCI_SESSION_DEFAULT);
            if (oci_connection == NULL) {
                rt_log_info ("Connect to CaseDB failure!(%s:%s:%s)", db->dbname, db->usrname, db->passwd);
                err_handler(OCI_GetLastError());
                return;
            }
            rt_log_info("%s", OCI_GetVersionServer(oci_connection));
            rt_log_info ("CaseDB Connected!");
        }
        if (!oci_state &&
            oci_connection) {
            oci_state = OCI_StatementCreate (oci_connection);
            if (oci_state == NULL) {
                err_handler (OCI_GetLastError());
                return;
            }
        }
    }
}

/****************************************************************************
 函数名称  : SGCDBWriter
 函数功能    : 注册的线程回调函数，接收处理消息队列cdb_mq的消息
 输入参数    : 指针param
 输出参数    : 无
 返回值     : 正常情况下不会退出
 创建人     : yuansheng
 备注      :
****************************************************************************/
void *SGCDBWriter(void *param)
{
    int s = 0;
    void *data = NULL;
    char sql[512] = {0};
    struct vpm_t            *vpm;
    struct vrs_trapper_t    *rte;
    struct counter_upload_t *_this;
    OCI_Statement *st = oci_state;
    OCI_Connection *cn = oci_connection;

    rte = (struct vrs_trapper_t *)param;
    vpm = rte->vpm;

    FOREVER
    {
        data = NULL;
        st = oci_state;
        cn = oci_connection;
        if (!cn || !st)
        {
            sleep(2);
            continue;
        }
        /** Recv from internal queue */
        rt_mq_recv (vpm->cdb_mq, &data, &s);
        if (unlikely(!data))
        {
            continue;
        }

        _this = (struct counter_upload_t *)data;
        snprintf(sql, 512, "insert into hitcount (id,time,hitsession,totalsession,vpwid) values(seq_hitcount.nextval, \'%s\', %d, %d, %d)",
                  _this->tm, _this->hitted_sum, _this->matched_sum, _this->vpw_id);
        rt_log_debug ("go to do sql[%s]", sql);
        //执行sql语句
        if (!OCI_ExecuteStmt(st, sql))
        {
                err_handler (OCI_GetLastError());
                goto memfree;
        }
        
        //提交
        if (!OCI_Commit(cn))
        {
                err_handler (OCI_GetLastError());
                goto memfree;
        }
memfree:
        kfree(data);
        data = NULL;
    }
    task_deregistry_id (pthread_self ());
    return NULL;
}

