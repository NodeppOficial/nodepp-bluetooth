#ifndef NODEPP_BSOCKET
#define NODEPP_BSOCKET

/*────────────────────────────────────────────────────────────────────────────*/

#if   _KERNEL == NODEPP_KERNEL_WINDOWS
    #include <nodepp/poll.h>
    #include <nodepp/socket.h>
    #include "windows/bluetooth.cpp"
#elif _KERNEL == NODEPP_KERNEL_POSIX
    #include <nodepp/poll.h>
    #include <nodepp/socket.h>
    #include "posix/bluetooth.cpp"
#else
    #error "This OS Does not support bluetooth.h"
#endif

/*────────────────────────────────────────────────────────────────────────────*/

#endif