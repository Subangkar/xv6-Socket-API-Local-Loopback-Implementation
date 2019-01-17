//
// Created by subangkar on 1/17/19.
//

#ifndef XV6_SOCKETAPI_SIGNALCODES_H
#define XV6_SOCKETAPI_SIGNALCODES_H

// socket function debug
//#define SO_ARG_DEBUG
#define SO_DEBUG
//#define SO_FUNC_DEBUG

#define NULL 0

// error code for missing required arguments
#define E_MISSING_ARG -1

// error codes for socket
#define E_NOTFOUND -1025
#define E_ACCESS_DENIED -1026
#define E_WRONG_STATE -1027
#define E_FAIL -1028
#define E_INVALID_ARG -1029

#endif //XV6_SOCKETAPI_SIGNALCODES_H
