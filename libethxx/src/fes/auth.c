/*
*   authentication.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: User Authentication
*   Personal.Q
*/

#include "sysdefs.h"
#include "auth.h"

char *auth_errmsg[] = _AUTH_ERRMSG_INIT;

auth_result_st spasr_user_authentication(login_request_st *auth)
{
    auth_result_st result = AUTH_TIMEDOUT;

    if (auth->manage == 0x01 && auth->optype == 0x01 &&
            !STRNCMP(auth->uname, AUTH_UNAME_STR, auth->uname_len) &&
            !STRNCMP(auth->pwd, AUTH_PWD_STR, auth->pwd_len))
    { result = AUTH_NERROR; }

    return result;
}
