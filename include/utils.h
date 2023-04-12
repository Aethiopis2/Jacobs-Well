//==============================================================================================================|
// Project Name:
//  Jacob's Well
//
// File Desc:
//  Basic utility functions
//
// Program Authors:
//  Rediet Worku, Dr. aka Aethiopis II ben Zahab       PanaceaSolutionsEth@gmail.com, aethiopis2rises@gmail.com
//
// Date Created:
//  23rd of March 2023, Thursday
//
// Last Updated:
//  23rd of March 2023, Thursday
//==============================================================================================================|
#ifndef UTILS_H
#define UTILS_H


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
/**
 * @brief 
 *  An application configuration is but a key-value pair. When BluNile starts it reads the info stored at 
 *  "config.dat" file and caches the items in memory for later referencing.
 */
typedef struct APP_CONFIG_FILE
{
    std::map<std::string, std::string> dat;
} APP_CONFIG, *APP_CONFIG_PTR;




//==============================================================================================================|
// GLOBALS
//==============================================================================================================|
extern char *buffer;               // a generalized storage buffer for receiveing 
extern char *snd_buffer;            // buffer used for sending custom-appended info


// command line overrides
extern int debug_mode;                  // enables debugging mode; default no display simply run mode
extern int backlog;                     // number of buffered conns (or so we're told, that's a suspicious fella!!!)
extern int buffer_size;                 // buffer size for buffer


extern u16 listen_port;





//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
void Dump_Hex(const char *p_buf, const size_t len);
int Read_Config(APP_CONFIG_PTR p_config, std::string filename);
void Split_String(const std::string &str, const char tokken, std::vector<std::string> &dest);
void Process_Command_Line(char **argv, const int argc, std::string &filename);



#endif
//==============================================================================================================|
//          THE END
//==============================================================================================================|