//==============================================================================================================|
// Project Name:
//  Jacob's Well
//
// File Desc:
//  The local-buddy connects to RESTServer or RESTful servers running on local-side of the equation possibly on
//  a debugger such as Visual Studio. The following steps help paint a clearer picture and see the functions of
//  local-buddy a lot-clearer. Note however, the steps here are in relation to WSIS:
//  1. Update "ConnectionStrings" section of WSIS app.config (RESTServer.dll.config) file as such,
//      1.1. Change "DataSource" part to the IP and port of the local-buddy.
//      1.2. Set "User Id" and "Password" parts to the credientials of the remote side (not local remember we
//              dont want to access our local database, we want to debug the remote one).
//      1.3. All other parts remain as they are
//  2. Update "config-local.dat" to its requirements; i.e. set up the remote-buddy ip:port and optionally its 
//      own listening port.
//
// Program Authors:
//  Rediet Worku, Dr. aka Aethiopis II ben Zahab       PanaceaSolutionsEth@gmail.com, aethiopis2rises@gmail.com
//
// Date Created:
//  19th of March 2023, Sunday
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
// GLOBALS
//==============================================================================================================|
int listen_fd{-1};
u16 listen_port{7777};
std::unordered_map<int, CONNECTION_INFO> remote_fd;     // map of server ip:port addresses to remote-buddy descriptor
std::unordered_map<int,std::string> fdip;               // map of fd to ip descriptor

bool bsend_close{true};     // direction of close




//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
void Init(const int argc, char **argv);
void Dump(const char *msg, ...);
void New_Remote(const int fd, const char *buf);
void New_Db(const int fd, const char *buf, const size_t len);
void Close_Sockets();
void Kill_Sock(const int fd);




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
    // for windows
    #if defined(WIN32) || defined(_WIN64)                                                                                    
        WSADATA wsa;        

        // request version 2.2 of the lib (the highest in WinSock lib)
        if (WSAStartup(MAKEWORD(2,2), &wsa)) {
            fprintf(stderr, "WSAStartup() returned error code: %d\n", WSAGetLastError());
            return 0;
        } // end if bad call
    #endif

    Init(argc, argv);

    // get me sockets for the remote side and local sides; for now lets make things simple
    //  by requesting IPv4 format on TCP layer; TCP/IPv4
    listen_fd = Socket();

    // force the reusing of address on linux systems & rset non blocking
    Tcp_Reuse_Addr(listen_fd);
    Tcp_NoDelay(listen_fd);

    // Bind and start listen
    Bind(listen_fd, listen_port);
    Listen(listen_fd, backlog);

    memset(&fdpoll, 0, sizeof(fdpoll));
    fdpoll.fd = listen_fd;
    fdpoll.events = POLLIN;
    vpoll.push_back(fdpoll);        // now add to the list of 'we'd wanna wait on descriptors'

    /* we don't really wanna stop, till the ends of time if possible ... */
    while (1)
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
                
                fdip.emplace(nfd, addr_str);
                Dump("connection request from host @ (%s:%d)", addr_str, port);
            } // end if listening
            else 
            {
                int fd = tempfd[i].fd;

                // here are the possiblites that go down as far as routing is concerned
                //  1. ADO.NET based database requests are coming from RESTServer (new or old)
                //  2. remote-buddy is sending client requests (new or old)
                //  3. remote-buddy is responding to ADO.NET db requests
                //  4. RESTServer is responding to client requests

                // check remote-buddy descriptors first
                auto it = remote_fd.find(fd);
                if (it != remote_fd.end())
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
                        Dump("got %d bytes from \033[32mremote-buddy\033[37m on socket %d.\n",
                             bytes + sizeof(intap), fd);
                        Dump_Hex((char*)&intap, sizeof(intap));
                        Dump_Hex(buffer, bytes);
                    } // end if debug_mode

                    // this is from a remote-buddy descriptor; remote-buddy always responds with
                    //  an appended custom protocol info, let's parse that.
                    if (!strncmp(intap.signature, "INTAP11", 8))
                    {
                        // this is from our remote side;
                        int id = NTOHS(intap.id);
                        switch (id)
                        {
                            case CMD_BYEBYE:    // socket sent FIN
                                bsend_close = false;
                                Kill_Sock(NTOHS(intap.dest_fd));
                                break;

                            case CMD_ECHO:  // just echoing on existing
                            {
                                int lfd = NTOHS(intap.dest_fd);
                                int rfd = NTOHS(intap.src_fd);
                                Send(lfd, buffer, bytes);

                                if (remote_fd[fd].mfds[lfd] == -1)
                                    remote_fd[fd].mfds[lfd] = rfd;
                            } break;

                            case CMD_CLI_CONNECT:   // new client connection
                            {
                                intap.port = NTOHS(intap.port);
                                Dump("connecting with RESTful server at %s:%d ..", intap.ip, intap.port);
                                int nfd = Socket();
                                Connect(nfd, intap.ip, intap.port);
                                Dump("connected to RESTful server at %s:%d", intap.ip, intap.port);

                                Send(nfd, buffer, bytes);
                                memset(&fdpoll, 0, sizeof(fdpoll));
                                fdpoll.fd = nfd;
                                fdpoll.events = POLLIN;
                                vpoll.push_back(fdpoll);
                                remote_fd[fd].mfds[nfd] = NTOHS(intap.src_fd);
                            } break;
                        } // end switch
                    } // end if intap
                    else
                    {
                        fprintf(stderr, "\033[31m> local-buddy:\033[37m no hablo comprende, error de protocolo!\n");
                    } // end else unkown protocol
                } // end if remote
                else
                {
                    // only two possiblities here; either we've seen this connection before
                    //  or we didn't. If nothing new then, we're sure to have mapped all the info we
                    //  need at this point. These could be requests from existing db connection
                    //  or responses from RESTServer (in which case descriptor is already connected)
                    
                    memset(buffer, 0, buffer_size);
                    int bytes = recv(fd, buffer, buffer_size, 0);
                    if (bytes <= 0)
                    {
                        Kill_Sock(fd);
                        continue;
                    } // end bytes

                    if (debug_mode & DEBUG_L3)
                    {
                        Dump("got %d bytes from one of my peers on socket %d.\n", bytes, fd);
                        Dump_Hex(buffer, bytes);
                    } // end if debug_mode

                    bool bfound{false};
                    for (auto &x : remote_fd)
                    {
                        auto it = x.second.mfds.find(fd);
                        if (it != x.second.mfds.end())
                        {
                            // simply echo, the response
                            Dump("echo response to \033[32mremote-buddy\033[37m");
                            INTAP_FMT intap;
                            intap.id = HTONS(CMD_ECHO);
                            intap.src_fd = HTONS(it->first);
                            intap.dest_fd = HTONS(it->second);
                            intap.buf_len = HTONL(bytes);

                            CPY_SND_BUFFER(x.first, snd_buffer, intap, buffer, bytes);
                            bfound = true;
                            break;
                        } // end if echo
                    } // end for

                    if (!bfound)
                    {
                        // this must be a new connection either from new remote or
                        //  ADO.NET client thinking I'm SQL Server, hehehhe ....

                        if (!strncmp(buffer, "INTAP11", 8))
                        {
                            if (NTOHS(((INTAP_FMT_PTR)buffer)->id) == CMD_HELLO)
                                New_Remote(fd, buffer);
                        } // end if
                        else
                        {
                            // dead in the eye, new Db connection
                            New_Db(fd, buffer, bytes);
                        } // end else
                    } // end if not found
                } // end else not remote
            } // end else not listening
        } // end for
    } // end while

    Close_Sockets();
    return 0;     
} // end main


//==============================================================================================================|
/**
 * @brief 
 *  prints welcome message and overrides command line params
 * 
 * @param argc 
 * @param argv 
 */
void Init(const int argc, char **argv)
{
    printf("\n*************************************************************\n");
    printf("*\tINTAPS local-buddy v1.3.0\n*\tcreated by: \033[31mRed\033[37miet \033[33mWorku\033[37m");
    printf("\n*************************************************************\n");

    // open the config file, but first test if we must override the filename
    Dump("intailizing ..");
    std::string dummy;
    Process_Command_Line(argv, argc, dummy);
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
        printf("\033[33m> local-buddy:\033[37m %s.\n", buf);
        va_end(arg_list);
    } // end if
} // end Dump


//==============================================================================================================|
/**
 * @brief 
 *  Begins a connection for first time with remote buddy; adds it to the list of remote-buddies that we wait on
 * 
 * @param [fd] the descriptor for remote-buddy 
 * @param [buffer] containing the received data 
 */
void New_Remote(const int fd, const char *buf)
{
    Dump("new \033[32mremote-buddy\033[37m connection");
    CONNECTION_INFO ci{};
    ci.ip = ((INTAP_FMT_PTR)buf)->ip;
    ci.port = NTOHS(((INTAP_FMT_PTR)buf)->port);
    
    remote_fd.emplace(fd, ci);
} // end Process_First_Time_Request


//==============================================================================================================|
/**
 * @brief 
 *  Handles the new database connection request; it locates the remote-buddy for sending using its ip and appends
 *  custom protocol info and sends it on the go.
 * 
 * @param [fd] the descriptor 
 * @param [buffer] buffer containing received data 
 * @param [len] length in bytes of the buffer above 
 */
void New_Db(const int fd, const char *buf, const size_t len)
{
    Dump("new connection request to RDBMS");
    for (auto &x : remote_fd)
    {
        if (!strncmp(fdip[fd].c_str(), x.second.ip.c_str(), fdip[fd].size()))
        {
            INTAP_FMT intap;
            intap.id = HTONS(CMD_DB_CONNECT);
            intap.src_fd = HTONS(fd);
            intap.dest_fd = HTONS(-1);
            intap.buf_len = HTONL(len);

            CPY_SND_BUFFER(x.first, snd_buffer, intap, buf, len);
            x.second.mfds.emplace(fd, -1);
            return;
        } // end if same
    } // end for

    // let's look for a remote descriptor with ip 0.0.0.0:0
    for (auto &x : remote_fd)
    {
        if (!strncmp(x.second.ip.c_str(), "0.0.0.0", x.second.ip.length()))
        {
            x.second.ip = fdip[fd];
            INTAP_FMT intap;
            intap.id = HTONS(CMD_DB_CONNECT);
            intap.src_fd = HTONS(fd);
            intap.dest_fd = HTONS(-1);
            intap.buf_len = HTONL(len);

            CPY_SND_BUFFER(x.first, snd_buffer, intap, buf, len);
            x.second.mfds.emplace(fd, -1);
            return;
        } // end if new db connection request with a new remote
    } // end for


    // at this point means an error
    fprintf(stderr, "\033[31m> local-buddy:\033[37m la problema, shouldn't get here!\n");
} // end New_Db


//==============================================================================================================|
/**
 * @brief 
 *  Closes all active descriptors
 */
void Close_Sockets()
{
    for (auto &x : remote_fd)
    {
        for (auto &y : x.second.mfds)
            Kill_Sock(y.first);

        x.second.mfds.clear();
        Kill_Sock(x.first);
    } // end for

    remote_fd.clear();
    CLOSE(listen_fd);
} // end Close_Sockets


//==============================================================================================================|
/**
 * @brief 
 *  kills the active descriptor on FIN signals
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
    
    auto it = remote_fd.find(fd);
    if (it != remote_fd.end())
    {
        if (bsend_close)
        {
            // only if this is self initated
            memcpy(snd_buffer, &intap, sizeof(intap));
            Send(fd, (const char *)&intap, sizeof(intap));
        } // end if send kill 

        CLOSE(it->first);
        remote_fd.erase(it);
    } // end if remote desc ending
    else
    {
        // this must be one of paired-descriptors let's end
        for (auto &x : remote_fd)
        {
            auto it2 = x.second.mfds.find(fd);
            if (it2 != x.second.mfds.end())
            {
                if (bsend_close)
                {
                    intap.dest_fd = HTONS(it2->second);
                    memcpy(snd_buffer, &intap, sizeof(intap));
                    Send(x.first, snd_buffer, sizeof(intap));
                } // end if sending kill

                CLOSE(it2->first);
                x.second.mfds.erase(it2);
            } // end for
        } // end foreach
    } // end else

    bsend_close = true;      // restore
    fdip.erase(fd);
    Erase_Sock(fd);
} // end Kill_Sock


//==============================================================================================================|
//          THE END
//==============================================================================================================|