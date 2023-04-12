//==============================================================================================================|
// Project Name:
//  Jacob's Well
//
// File Desc:
// 
//
// Program Authors:
//  Rediet Worku, Dr. aka Aethiopis II ben Zahab       PanaceaSolutionsEth@gmail.com, aethiopis2rises@gmail.com
//
// Date Created:
//  20th of March 2023, Monday
//
// Last Updated:
//  6th of April 2023, Thursday
//
//==============================================================================================================|



//==============================================================================================================|
// INCLUDES
//==============================================================================================================|
#include "utils.h"



//==============================================================================================================|
// DEFINES
//==============================================================================================================|





//==============================================================================================================|
// TYPES
//==============================================================================================================|
// a little structure that helps sync request...responses -- WSIS clients have the tendency to send requests without
//  awiating for responses during poll() or select().
typedef struct SOCK_WAIT_FMT
{
    int fd;                 // a descriptor that's on the left side
    bool brequest{true};    // indicates that its ready to process requests from clients
} MI_SOCK_WAIT, *MI_SOCK_WAIT_PTR;





//==============================================================================================================|
// GLOBALS
//===================================================================================================
int listen_fd{-1};                      // the listening descriptor (one ring to rule them all)
u16 listen_port{8888};                  // the port for listening server
int local_fd = -1;                      // descriptor to local-buddy
std::unordered_map<int, MI_SOCK_WAIT> mfds;      // map of remote-buddy to local-buddy descriptors

std::string server_ip,      // ip address of RESTful server
            db_ip,          // ip address of database
            local_ip;       // ip address of local buddy
u16 server_port,            // port for RESTful server
    db_port,                // port for database server
    local_port;             // port for local-buddy

bool bsend_close{true};      // direction of close



//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
void Init(int argc, char **argv);
inline void Hello_Buddy();
void New_Db(const int fd, const char *pbuf, const INTAP_FMT_PTR pintap, const size_t len);
void Close_Sockets();
void Kill_Sock(const int fd);
inline void Dump(const char *msg, ...);



//==============================================================================================================|
// FUNCTIONS
//==============================================================================================================|
/**
 * @brief 
 *  Program entry point
 * 
 */
int main(int argc, char *argv[])
{
    // for windows, load winsock libraries which are not default on such systems
    #if defined(WIN32) || defined(_WIN64)                                                                                    
        WSADATA wsa;        

        // request version 2.2 of the lib (the highest in WinSock lib)
        if (WSAStartup(MAKEWORD(2,2), &wsa)) {
            fprintf(stderr, "WSAStartup() returned error code: %d\n", WSAGetLastError());
            return 0;
        } // end if bad call
    #endif

    // Initalize                
    Init(argc, argv);

    // start connecting with local buddy
    Dump("connecting with \033[33mlocal-buddy\033[37m ..");
    Hello_Buddy();
    Dump("connected to \033[33mlocal-buddy\033[37m");

    
    // get me sockets for the remote side and local sides; for now lets make things simple
    //  by requesting IPv4 format on TCP layer; TCP/IPv4
    listen_fd = Socket();

    // force the reusing of address on linux systems
    Tcp_Reuse_Addr(listen_fd);
    Tcp_NoDelay(listen_fd);

    // Bind and start listen
    Bind(listen_fd, listen_port);
    Listen(listen_fd, backlog);

    memset(&fdpoll, 0, sizeof(fdpoll));
    fdpoll.fd = listen_fd;
    fdpoll.events = POLLIN;
    vpoll.push_back(fdpoll);        // now add to the list of 'we'd wanna wait on descriptors'

    fdpoll.fd = local_fd;
    vpoll.push_back(fdpoll); 

    while (true)
    {
        Dump("waiting for ready sockets ..");
        if ( POLL(vpoll.data(), vpoll.size()) <= 0 )
        {
            perror("poll()");
            break;
        } // end if poll error

        // print descriptors
        if (debug_mode & DEBUG_L2)
        {
            Dump("sockets =");
            for (auto &x : vpoll)
                printf("%d, ", x.fd);
            printf("\n");
        } // end if print descriptors 
            
        // a descriptor is ready, but which one?
        //  make copies to make life easy for our descriptors
        std::vector<struct pollfd> tempfd{vpoll};
        for (size_t i = 0; i < tempfd.size(); i++)
        {
            if (tempfd[i].revents == 0)
                continue;
            
            if (!(tempfd[i].revents & POLLIN))
            {
                Kill_Sock(tempfd[i].fd);
                continue;
            } // end if

            if (tempfd[i].fd == listen_fd)
            {
                char addr_str[INET_ADDRSTRLEN];
                u16 port;
                int nfd = Accept(listen_fd, addr_str, port);
                if (nfd <= 0)
                    continue;

                Dump("accepted new connection from host (%s:%d)", addr_str, port);
            } // end if listening
            else 
            {
                int fd = tempfd[i].fd;
                
                // What do we know at this point? In much the same way as local-buddy, remote-buddy too
                //  expects connections from these sources
                //  1. from local-buddy (these are always appended with INTAP, a custom protocol)
                //  2. from clients (either a new one or over exsiting)
                //  3. from database responses -- the connection must exist (since its a response)

                // maybe this is the local-buddy?
                if (fd == local_fd)
                {
                    INTAP_FMT intap;
                    int bytes = Recv(fd, (char*)&intap, sizeof(intap));
                    if (bytes <= 0)
                    {
                        Kill_Sock(fd);
                        continue;
                    } // end bytes

                    // now get the actual info we need
                    if ( (bytes = Recv(fd, buffer, NTOHL(intap.buf_len))) <=  0)
                    {
                        Kill_Sock(fd);
                        continue;
                    } // end if

                    if (debug_mode & DEBUG_L3)
                    {
                        Dump("got total bytes %d from \033[32mlocal-buddy\033[37m on socket %d", 
                            bytes + sizeof(intap), fd);
                        Dump_Hex((char*)&intap, sizeof(intap));
                        Dump_Hex(buffer, bytes);
                    } // end if debug_mode

                    // its either the clients or db responses that's what we get here
                    if (!strncmp(intap.signature, "INTAP11", 8))
                    {
                        int id = NTOHS(intap.id);
                        switch (id)
                        {
                            case CMD_BYEBYE:    // closing are we
                                bsend_close = false;
                                if (NTOHS(intap.dest_fd) > 0)
                                    Kill_Sock(NTOHS(intap.dest_fd));
                                else
                                    Kill_Sock(fd);
                                break;

                            case CMD_DB_CONNECT:    // new db connection
                                New_Db(fd, buffer, &intap, bytes);
                                break;

                            case CMD_ECHO:  // routing as is
                            {
                                int lfd = NTOHS(intap.dest_fd);
                                int rfd = NTOHS(intap.src_fd);

                                Send(lfd, buffer, bytes);
                                if (strstr(buffer, "HTTP/1.1 100 Continue"))
                                    mfds[lfd].brequest = true;

                                if (mfds[lfd].fd <= 0)
                                    mfds[lfd].fd = rfd;
                            } break;
                        } // end switch
                    } // end if intap
                    else 
                    {
                        fprintf(stderr, "\033[31m> remote-buddy:\033[37m mi dispiace, errore di protocolo!\n");
                    } // end else
                } // end if local-buddy
                else
                {
                    // a database response or a client request? which one? would be up to you ...
                    // but from the descriptor side we can view it as new connection or existing.
                    auto it = mfds.find(fd);
                        if (it != mfds.end())
                            if (!mfds[fd].brequest)
                                continue;

                    memset(buffer, 0, buffer_size);
                    int bytes = recv(fd, buffer, buffer_size, 0);
                    if (bytes <= 0)
                    {
                        Kill_Sock(fd);
                        continue;
                    } // end bytes

                    if (debug_mode & DEBUG_L3)
                    {
                        Dump("got total bytes %d from peer on socket %d", bytes, fd);
                        Dump_Hex(buffer, bytes);
                    } // end if debug_mode

                    if (it != mfds.end())
                    {
                        Dump("routing to \033[33mlocal-buddy\033[37m");

                        INTAP_FMT intap;
                        intap.id = HTONS(CMD_ECHO);
                        intap.src_fd = HTONS(it->first);
                        intap.dest_fd = HTONS(it->second.fd);
                        intap.buf_len = HTONL(bytes);

                        CPY_SND_BUFFER(local_fd, snd_buffer, intap, buffer, bytes);
                        if (strstr(buffer, "Expect: 100-continue"))
                            it->second.brequest = false;
                    } // end if existing
                    else
                    {
                        Dump("new client request");
                        INTAP_FMT intap;
                        MI_SOCK_WAIT sw{-1, true};

                        intap.id = HTONS(CMD_CLI_CONNECT);
                        intap.src_fd = HTONS(fd);
                        intap.buf_len = HTONL(bytes);
                        intap.port = HTONS(server_port);
                        strncpy(intap.ip, server_ip.c_str(), 
                            (server_ip.length() > INET_ADDRSTRLEN ? INET_ADDRSTRLEN : server_ip.length()) );
                        CPY_SND_BUFFER(local_fd, snd_buffer, intap, buffer, bytes);

                        if (strstr(buffer, "Expect: 100-continue"))
                            sw.brequest = false;

                        mfds[fd] = sw;
                    } // end else new client request
                } // end else not local
            } // end else
        } // end for
    } // end while

    Close_Sockets();
    return 0;     
} // end main

//==============================================================================================================|
/**
 * @brief 
 *  Initalizes the globals and overrides some default paramters from command line
 * 
 * @param [argc] command line argument count 
 * @param [argv] command line arguments 
 */
void Init(int argc, char **argv)
{
    APP_CONFIG config;
    std::string filename{"config.dat"};

    printf("\n*************************************************************\n");
    printf("*\tINTAPS remote-buddy v1.3.0\n*\tcreated by: \033[31mRed\033[37miet \033[33mWorku\033[37m");
    printf("\n*************************************************************\n");

    // handle any extra-command line arguments
    Process_Command_Line(argv, argc, filename);

    // initalize the configuration info
    Read_Config(&config, filename);

    // set values
    std::vector<std::string> dest;
    listen_port = atoi(config.dat["Listen_Port"].c_str());

    Split_String(config.dat["RESTServer_Address"], ':', dest);
    server_ip = dest[0];
    server_port = atoi(dest[1].c_str());
    dest.clear();

    Split_String(config.dat["Database_Address"], ':', dest);
    db_ip = dest[0];
    db_port = atoi(dest[1].c_str());
    dest.clear();

    Split_String(config.dat["Local_Buddy"], ':', dest);
    local_ip = dest[0];
    local_port = atoi(dest[1].c_str());
} // end Init


//==============================================================================================================|
/**
 * @brief 
 *  Dumps the message to console (stdout) if we are allowed to do so; i.e. degbug_mode is set to DEBUG_L1 or 
 *  above.
 * 
 * @param [msg] the message to dump 
 */
inline void Dump(const char* msg, ...)
{
    char buf[BUF_SIZE];
    va_list arg_list;

    if (debug_mode & DEBUG_L1)
    {
        va_start(arg_list, msg);
        vsnprintf(buf, BUF_SIZE, msg, arg_list);
        printf("\033[32m> remote-buddy:\033[37m %s.\n", buf);
        va_end(arg_list);
    } // end if
} // end Dump


//==============================================================================================================|
/**
 * @brief 
 *  starts connection and sends an intial 'hello' message to local-buddy to let it know what's up
 */
inline void Hello_Buddy()
{
    INTAP_FMT intap;
   
    local_fd = Socket();
    Connect(local_fd, local_ip.c_str(), local_port);
    Tcp_NoDelay(local_fd);

    intap.id = HTONS(CMD_HELLO);
    intap.port = HTONS(0);
    intap.src_fd = HTONS(local_fd);
    intap.dest_fd = HTONS(-1);
    intap.buf_len = 0;
    strncpy(intap.ip, "0.0.0.0", 8);

    Send(local_fd, (char*)&intap, sizeof(intap));
} // end Process_First_Time_Request


//==============================================================================================================|
/**
 * @brief 
 *  Connects to a RDBMS server instance and sends whatever it got from local-buddy
 * 
 * @param [fd] local-buddy descriptor 
 * @param [pbuf] buffer containing data 
 * @param [pintap] pointer to intap structure
 * @param [len] length of buffer 
 */
void New_Db(const int fd, const char *pbuf, const INTAP_FMT_PTR pintap, const size_t len)
{
    Dump("connecting to RDBMS ..");
    int dbfd = Socket();
    Connect(dbfd, db_ip.c_str(), db_port);
    Dump("Connected with RDBMS");

    Send(dbfd, pbuf, len);
    
    memset(&fdpoll, 0, sizeof(fdpoll));
    fdpoll.fd = dbfd;
    fdpoll.events = POLLIN;
    vpoll.push_back(fdpoll);

    MI_SOCK_WAIT sw{NTOHS(pintap->src_fd), true};
    mfds[dbfd] = sw;
} // end New_Db


//==============================================================================================================|
/**
 * @brief 
 *  Closes all active descriptors
 */
void Close_Sockets()
{
    for (auto &x : mfds)
        Kill_Sock(x.first);

    mfds.clear();
    CLOSE(listen_fd);
} // end Close_Sockets


//==============================================================================================================|
/**
 * @brief 
 *  kills the active descriptor on FIN signal
 * 
 * @param [fd] descriptor identifer 
 */
void Kill_Sock(const int fd)
{
    INTAP_FMT intap;
    intap.id = HTONS(CMD_BYEBYE);
    intap.src_fd = HTONS(fd);
    intap.dest_fd = HTONS(-1);
    intap.buf_len = 0;


    Dump("killin' em softly, socket %d", fd);
    auto it = mfds.find(fd);
    if (it != mfds.end())
    {
        if (bsend_close)
        {
            intap.dest_fd = HTONS(it->second.fd);
            Send(local_fd, (const char *)&intap, sizeof(intap));
        } // end if sending kill

        CLOSE(it->first);
        mfds.erase(it);
        Erase_Sock(fd);
    } // end if

    if (fd == local_fd)
    {
        CLOSE(local_fd);
        Erase_Sock(fd);
    } // end if

    bsend_close = true;     // back to normal
} // end Kill_Sock


//==============================================================================================================|
//          THE END
//==============================================================================================================|