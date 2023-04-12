//==============================================================================================================|
// Project Name:
//  Jacob's Well
//
// File Desc:
//  Basic net wrappers and some utility functions
//
// Program Authors:
//  Rediet Worku, Dr. aka Aethiopis II ben Zahab       PanaceaSolutionsEth@gmail.com, aethiopis2rises@gmail.com
//
// Date Created:
//  13th of Feburary 2023, Monday
//
// Last Updated:
//  13th of Feburary 2023, Monday
//
// NOTES:
//  In linux using g++ (however choice C/C++ compiler is not imposed) compile as (in realse mode):
//      g++ -02 remote-buddy.cpp -o remote-buddy
//==============================================================================================================|
#ifndef NET_WRAPPERS_H
#define NET_WRAPPERS_H


#if defined(WIN32) || defined(_WIN64)
#pragma comment(lib, "WS2_32.lib")     // Windows systems require this
#endif


//==============================================================================================================|
// INCLUDES
//==============================================================================================================|
#include <string>               // C++ style strings
#include <sstream>              // stream processors
#include <vector>               // take a wild guess
#include <map>                  // C++ maps
#include <unordered_map>        // C++ maps
#include <algorithm>            // many important iterator and algorithims
#include <fstream>              // C++ file streams
#include <iomanip>              // C++ formatting


#include <sys/types.h>          // some C style types
#include <stdio.h>              // basic IO lib (C style)
#include <string.h>             // C style strings and many impt C string libs
#include <errno.h>              // defines the global errno
#include <stdarg.h>             // ANSI C header file 
#include <inttypes.h>           // defines some platform types
#include <thread>               // C++ 11 cross-platform threads


#if defined (__unix__) || defined (__linux__)
#include <sys/socket.h>         /* basic socket functions */
#include <sys/poll.h>           /* defines poll() call */
#include <sys/ioctl.h>          /* impt io control functions */
#include <sys/time.h>           /* time_val {} for select */
#include <netinet/tcp.h>        /* some low level tcp stuff */
#include <netinet/in.h>         /* sockaddr_in {} definition */
#include <arpa/inet.h>          /* inet(3) functions */
#include <unistd.h>             /* many unix system calls */
#include <netdb.h>              /* extended net defintions */

#define CLOSE(s)        close(s);
#define POLL(ps, len)   poll(ps, len, -1)
#else 
#if defined(WIN32) || defined(_WIN64)
#include <WinSock2.h>           /* windows socket library */
#include <ws2tcpip.h>           /* some more socket functions */

#define CLOSE(s)        closesocket(s)
#define POLL(ps, len)   WSAPoll(ps, len, -1)
#endif
#endif


//==============================================================================================================|
// DEFINES
//==============================================================================================================|
// basic type redefinitions
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;



// misc
#define BUF_SIZE        2048       // buffer size used for sending and receving




// some endian arragments; we'd want to be using these functions no problem regardless
// of the machine architecture be it big endian or little
#if __BIG_ENDIAN__
#define HTONS(x)    (x)         /* 16-bit version */
#define HTONL(x)    (x)         /* 32-bit version */
#define HTONLL(x)   (x)         /* 64-bit version */

#define NTHOS(x)    (x)
#define NTHOL(x)    (x)
#define NTHOLL(x)   (x)
#else
#define HTONS(x)    htons(x)
#define HTONL(x)    htonl(x)
#define HTONLL(x)   (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))

// these reverse orders back into the host format; i.e. little-endian
#define NTOHS(x)    ntohs(x)
#define NTOHL(x)    ntohl(x)
#define NTOHLL(x)   (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif


// INTAP id's aka commands
#define CMD_HELLO         1
#define CMD_BYEBYE        2
#define CMD_DB_CONNECT    3
#define CMD_CLI_CONNECT   4
#define CMD_ECHO          5




// debugging levels; basically tell the app what we can and can't print (overrideable from command-line)
#define DEBUG_NORMAL    0       // no printing just stright up routing
#define DEBUG_L1        1       // level 1 debugging; just print some of the contents
#define DEBUG_L2        2       // level 2 extra-printing
#define DEBUG_L3        4       // level 3 print Dump-Hex along each routing (slow).



#define CPY_SND_BUFFER(fd, snd, __intap, __buffer, __bytes)  {\
    memcpy(snd, &__intap, sizeof(__intap)); \
    memcpy(snd + sizeof(__intap), __buffer, __bytes); \
    Send(fd, snd, sizeof(__intap) + __bytes); \
}



//==============================================================================================================|
// TYPES
//==============================================================================================================|
/**
 * @brief 
 *  A custom protocol; INTAP acronynm for INTAPS Network Transfer and Access Protocol.
 */
#pragma pack(1)
typedef struct INTAP_PROTO_FMT
{
    const char signature[8]{"INTAP11"};     // a protocol identifier; our custom protocol
    u16 id;                                 // tells remote what to do.
    s16 src_fd;                             // source descriptor; i.e. socket for this process
    s16 dest_fd;                            // the destination descriptor
    u16 port;                               // the port address in network order
    char ip[INET_ADDRSTRLEN];               // stores the ip for RESTServer (only one allowed per machine)

    // extensions; now officially INTAPv1.1
    u32 buf_len;        // the number of bytes down-below
} INTAP_FMT, *INTAP_FMT_PTR;



/**
 * @brief 
 *  the 'local-buddy' is meant to handle multiple remote-buddies at once each one using only one process
 *  per machine; this structure holds a connection info for each remote-buddy connection which contains
 *  multiple descriptors (since we'd be somewhat emulating servers to some-degree)
 */
typedef struct CONNECTION_INFO_FMT
{
    std::string ip;                     // ip address of RESTServer
    u16 port;                           // the coresponding port # (in network-byte-order)
    std::unordered_map<int, int> mfds;  // map of db descriptors (local -> foreign)
} CONNECTION_INFO, *CONNECTION_INFO_PTR;



//==============================================================================================================|
// GLOBALS
//==============================================================================================================|
extern std::vector<struct pollfd> vpoll;       // vector of poll structus
extern struct pollfd fdpoll;                   // a generalized storage




//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
int Socket();
void Connect(int fds, const char *ip, const u16 port);
void Bind(int fds, const u16 port);
void Listen(int fds, int backlog);
int Accept(const int listen_fd, char* addr_str, u16 &port);
void Send(int fds, const char *buf, const size_t buf_len);
int Recv(int fds, char *buf, const size_t buf_len);
void Select(int maxfdp, fd_set &rset);
void Set_Non_Blocking(int fd);
void Tcp_Reuse_Addr(const int lfd);
void Tcp_Keep_Alive(const int fd);
int Tcp_NoDelay(const int fds);
int Set_RecvTimeout(const int fds, const int sec=3);
void Erase_Sock(const int fd);



#endif
//==============================================================================================================|
//          THE END
//==============================================================================================================|