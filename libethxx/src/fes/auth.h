/*
*   authentication.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: User Authentication
*   Personal.Q
*/

#ifndef __AUTH_H__
#define __AUTH_H__

#define UNAME_LEN                           16
#define PASSWD_LEN                          16
#define AUTH_UNAME_STR                          "admin"
#define AUTH_PWD_STR                            "admin"

typedef struct
{
    unsigned char   manage;                 /** 0x01 */
    unsigned char   optype;                 /** 0x01, login request */
    unsigned char   uname_len;
    unsigned char   uname[UNAME_LEN];
    unsigned char   pwd_len;
    unsigned char   pwd[PASSWD_LEN];
} login_request_st;


typedef struct
{
    unsigned char   manage;     /** 0x01 */
    unsigned char   optype;     /** 0x02, login response */
    unsigned char   result;     /** 0x80 or 0x86 -> server set it */
    unsigned char   reason;     /** reason of login failure if result is 0x86: 0x01-username; 0x02-password; 0x03-other */
} login_ack_st;




typedef enum
{
    AUTH_NERROR     =   0,      /** successful */
    AUTH_UNAME      =   -1,     /** wrong user name */
    AUTH_PWD        =   -2,     /** wrong passwd */
    AUTH_TIMEDOUT   =   -3,     /** auth request timeout */
    /*
        Add more here
    */

    AUTH_LIMIT      /** Undefined */
} auth_result_st;

extern char *auth_errmsg[];


#define _AUTH_ERRMSG_INIT { \
        "Authentication successful\n",      /** E_NONE */       \
        "Failed user authentication \n",    /** E_UNAME */      \
        "Failed password authentication\n", /** E_PWD */        \
        "Authentication timeout\n",         /** E_TIMEDOUT */   \
        "Unknown error"                     /** E_LIMIT */      \
    }

#define _SHR_AUTH_ERRMSG(r) \
    auth_errmsg[(((int)r) <= 0 && ((int)r) > AUTH_NERROR) ? -(r) : -AUTH_LIMIT]


static const login_ack_st spasr_login_no_error_ack =
{
    .manage = 0x01,
    .optype = 0x02,
    .result = 0x80,
    .reason = 0x00,
};



extern auth_result_st spasr_user_authentication(login_request_st *auth);

#endif
