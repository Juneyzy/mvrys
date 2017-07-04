/*
*   lwrpc-config.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Light Weight Remote Procedure Call Configuration File
*   Personal.Q
*/

#ifndef __LWRPC_CONFIG_H__
#define __LWRPC_CONFIG_H__

    /* Do we need degbug */
#define RPC_DEBUG
    /* Do we need check rpc packet */
#define RPC_PKT_CHECK       0
    /* Loopback enable ctrl */
#define LOOPBACK_ENA 1
    /* Rpc version */
#define RPC_VERSION         0x1

#define RPC_PORT                20000

    //#define RPC_DEBUG_ENA

#endif

