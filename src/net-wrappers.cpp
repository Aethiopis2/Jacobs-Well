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
#if defined(WIN32) || defined(_WIN64)
#pragma comment(lib, "WS2_32.lib")     // Windows systems require this
#endif


//==============================================================================================================|
// INCLUDES
//==============================================================================================================|
#include "net-wrappers.h"


//==============================================================================================================|
// DEFINES
//==============================================================================================================|




//==============================================================================================================|
// TYPES
//==============================================================================================================|




//==============================================================================================================|
// GLOBALS
//==============================================================================================================|
std::vector<struct pollfd> vpoll;       // vector of poll structus
struct pollfd fdpoll;                   // a generalized storage




//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
/**
 * @brief 
 *  Creates a standard UNIX socket on TCP/IPv4 and returns a descriptor on success
 * 
 * @return int 
 *  a descriptor to socket
 */
int Socket()
{
    int fds;
    if ( (fds = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(0);
    } // end if

    return fds;
} // end Socket


//==============================================================================================================|
/**
 * @brief 
 *  Start's a TCP connection with peer on IPv4 (Keepin' it simple for now); kills the app on error
 * 
 * @param [fds] the descriptor to connect 
 * @param [ip] the ip address 
 * @param [port] the the port number 
 */
void Connect(int fds, const char *ip, const u16 port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;      // IPv4 family
    addr.sin_port = HTONS(port);    // port # in network-byte-order
    if (inet_pton(AF_INET, ip, &addr.sin_addr) < 0)
    {
        perror("invalid address");
        exit(0);
    } // end if no good address

    if (connect(fds, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(0);
    } // end if
} // end connect


//==============================================================================================================|
/**
 * @brief 
 *  Binds and a socket for server listening mode
 * 
 * @param [fds] the socket descriptor 
 * @param [port] the port # 
 */
void Bind(int fds, const u16 port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = HTONS(port);
    addr.sin_addr.s_addr = INADDR_ANY;  // listen on all interfaces

    if (bind(fds, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(0);
    } // end if
} // end Bind


//==============================================================================================================|
/**
 * @brief 
 *  puts a bounded interface on a listening mode
 * 
 * @param [fds] the listening descriptor and bounded to an interface 
 * @param [backlog] number of buffered connections
 */
void Listen(int fds, int backlog)
{
    if (listen(fds, backlog) < 0)
    {
        perror("listen");
        exit(0);
    } // end if
} // end Listen


//==============================================================================================================|
/**
 * @brief 
 *  Accepts an incomming connection on a listening interface
 */
int Accept(const int listen_fd, char *addr_str, u16 &port)
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int fd = accept(listen_fd, (sockaddr *)&addr, &len);
    if (fd <= 0)
    {
        perror("accept()");
        return -1;
    } // end if 

    if (!inet_ntop(AF_INET, (char*)&addr.sin_addr, addr_str, INET_ADDRSTRLEN))
    {
        perror("address conversion.\n");
        return -1;
    } // end if

    port = NTOHS(addr.sin_port);
    Tcp_NoDelay(fd);
    Set_RecvTimeout(fd, 3);

    memset(&fdpoll, 0, sizeof(fdpoll));
    fdpoll.fd = fd;
    fdpoll.events = POLLIN;
    vpoll.push_back({fd, POLLIN, 0});

    return fd;
} // end Accept


//==============================================================================================================|
/**
 * @brief 
 *  Sends the contents of the buffer over the connected socket
 * 
 * @param [fds] a descriptor 
 * @param [buf] buffer containing data 
 * @param [buf_len] length of buffer 
 */
void Send(int fds, const char *buf, const size_t buf_len)
{
    size_t bytes{0};      // sent thus far
    char *alias = (char *)buf;

    do {
        bytes = send(fds, alias, buf_len - bytes, 0);

        if (bytes < 0)
        {
            perror("send");
            exit(0);
        } // end if bytes

        alias += bytes;
    } while (bytes < buf_len);
} // end Send


//==============================================================================================================|
/**
 * @brief 
 *  Recievs incomming signal from peer on a given socket
 * 
 * @param [fds] the descriptor 
 * @param [buf] the buffer to get the data 
 * @param [buf_len] the length of buffer
 *  
 * @return int
 *  the number of bytes actually read 
 */
int Recv(int fds, char *buf, const size_t buf_len)
{
    int bytes, total_bytes{0};
    char *alias = buf;

    do 
    {
        bytes = recv(fds, alias, buf_len - total_bytes, 0);
        if (bytes == -1)
        {
            perror("recv");
            break;
        } // end if error or so

        total_bytes += bytes;
        alias += bytes;
    } while (total_bytes < (int)buf_len); // end while

    return total_bytes;
} // end Recv


//==============================================================================================================|
/**
 * @brief 
 *  Select based async socket multiplexing
 * 
 * @param [maxfdp] maximum descriptor 
 * @param [rset] the reading sets 
 */
void Select(int maxfdp, fd_set &rset)
{
    if (select(maxfdp, &rset, NULL, NULL, NULL) <= 0)
    {
        perror("select");
        exit(EXIT_FAILURE);
    } // end if
} // end Select


//==============================================================================================================|
/**
 * @brief 
 *  Helps certain linux/unix system to reuse the existing socket whenerver system restarts. This is usually 
 *  applied to a listening descriptor.
 * 
 * @param [lfd] listening socket 
 */
void Tcp_Reuse_Addr(const int lfd)
{
    u32 on{1};
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0)
    {
        perror("setsockopt()");
        exit(EXIT_FAILURE);
    } // end if socket option failure
} // end Tcp_Reuse_Addr


//==============================================================================================================|
/**
 * @brief 
 *  Sets a socket as a non-blocking mode
 * 
 * @param [fd] the descriptor to set as non-blocking 
 */
void Set_Non_Blocking(int fd)
{
#if defined (__unix__) || defined (__linux__)
    u32 on = 1;
    if (ioctl(fd, FIONBIO, (char *)&on) < 0)
        perror("ioctl() failed");
#elif defined (WIN32) || defined (_WIN64)
    unsigned long i_mode = 1;
    if (ioctlsocket(fd, FIONBIO, &i_mode))
        fprintf(stderr, "ioctlsocket() fail");
#endif
} // end Set_Non_Blocking


//==============================================================================================================|
/**
 * @brief 
 *  Turns on the keep alive heart-beat signal
 * 
 * @param [fd] the descriptor to keep-alive 
 */
void Tcp_Keep_Alive(const int fd)
{
    u32 on{1};
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on)) < 0)
    {
        perror("setsocketopt()");
        exit(EXIT_FAILURE);
    } // end if
} // end Tcp_Keep_Alive


//==============================================================================================================|
/**
 * @brief 
 *  Toggles the Tcp_NoDelay on/off; i.e. the Negel's algorithim (don't know what that is yet ...)
 * 
 * @param [fds] the socket descriptor 
 *  
 * @return int 
 *  a 0 on success alas -1
 */
int Tcp_NoDelay(const int fds)
{
#if defined(__unix__) || defined(__linux__)
    u32 flag{1};
    if (setsockopt(fds, SOL_TCP, TCP_NODELAY, (void*)&flag, sizeof(flag)) < 0)
        return -1;
#endif
    return 0;
} // end Tcp_NoDelay


//==============================================================================================================|
/**
 * @brief 
 *  Specifies the socket to time out in the number of seconds provided, this is useful for avoiding blocking
 *  recv calls, but there are better ways for that.
 * 
 * @param [fds] the socket descriptor 
 *  
 * @return int
 *  a 0 on successful setting of the timeout, else -1 on fail 
 */
int Set_RecvTimeout(const int fds, const int sec)
{
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = 0;

    // set socket options
    if (setsockopt(fds, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        return -1;
    
    return 0;
} // end Set_Recv_Timeout


//==============================================================================================================|
/**
 * @brief 
 *  Removes the descriptor
 * 
 * @param fd 
 */
void Erase_Sock(const int fd)
{
    // scan the socket from vector by its descriptor value
    auto it = std::find_if(vpoll.begin(), vpoll.end(), [&fd](auto &x) { return x.fd == fd; });
    if (it != std::end(vpoll))
        vpoll.erase(it);
} // end Erase_Sock


//==============================================================================================================|
//          THE END
//==============================================================================================================|