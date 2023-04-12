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
//  23rd of March 2023, Thursday
//
// Last Updated:
//  23rd of March 2023, Thursday
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




//==============================================================================================================|
// GLOBALS
//==============================================================================================================|
char *buffer;               // a generalized storage buffer for receiveing 
char *snd_buffer;


// command line overrides
int debug_mode;                  // enables debugging mode; default no display simply run mode
int backlog{5};                  // number of buffered conns (or so we're told, that's a suspicious fella!!!)
int buffer_size{BUF_SIZE};       // size of storage for buffer above






//==============================================================================================================|
// PROTOTYPES
//==============================================================================================================|
/**
 * @brief 
 *  Prints the contents of buffer in hex notation along side it's ASCII form much like hex viewer's do it.
 * 
 * @param ps_buffer 
 *  the information to dump as hex and char arrary treated as a char array.
 * @param len 
 *  the length of the buffer above
 */
void Dump_Hex(const char *p_buf, const size_t len)
{
    size_t i, j;                    // loop var
    const size_t skip = 8;          // loop skiping value
    size_t remaining = skip;        // remaining rows

    if (len > 65536)
        return;
    
    // print header
    printf("\n      ");
    for (i = 0; i < 8; i++)
        printf("\033[36m%02X ", (uint16_t)i);
    printf("\033[37m\n");

    if (len < skip)
        remaining = len;
    
    for (i = 0; i < len; i+=skip)
    {   
        printf("\033[36m%04X:\033[37m ", (uint16_t)i);
        for (j = 0; j < remaining; j++)
            printf("%02X ", (uint8_t)p_buf[i+j]);

        if (remaining < skip) {
            // fill blanks
            for (j = 0; j < skip - remaining; j++)
                printf("   ");
        } // end if

        printf("\t");

        for (j = 0; j < remaining; j++)
            printf("%c. ", p_buf[i+j]);

        printf("\n");

        if (len - (i + j) < skip)
            remaining = len - (i + j);
    } // end for

    printf("\n");
} // end Dump_Hex


//==============================================================================================================|
/**
 * @brief 
 *  a configuration file in WSIS BluNile is but a key-value pair, and C++ maps allow just to do that. The function
 *  opens a pre-set (canned) configuration file and reads its contents into a structure used to hold the values
 *  in memory for later referencing. This function should be called at startup.
 * 
 * @param [p_config] a buffer to store the contents of configuration file; APP_CONFIG type contains a map member
 *  that is used to store the actual configuration key,value pairs
 * 
 * @return int a 0 indicates success (or nothing happened as in parameter is null), alas -1 for error
 */
int Read_Config(APP_CONFIG_PTR p_config, std::string filename)
{
    // reject false calls
    if (!p_config)
        return 0;

    // open file for reading only this time
    std::ifstream config_file{filename};
    if (!config_file)
    {
        fprintf(stderr, "I think file is not there or I can't find it!\n");
        exit(EXIT_FAILURE);
    } // end if no file ope

    
    // some storage buffers, and read in the file till the end
    std::string key, value;
    while (config_file >> quoted(key) >> quoted(value))
        p_config->dat[key] = value;

    
    return 0;
} // end Read_Config


//==============================================================================================================|
/**
 * @brief 
 *  this function splits a string at the positions held by "tokken" which is a character pointing where to split
 *  string. The function uses a std::getline to split and store the results in a vector of strings passed as 
 *  parameter 3.
 * 
 * @param [str] the buffer containing the strings to split
 * @param [tokken] the character marking the positions of split in string
 * @param [dest] the destination vector taking on all the splitted strings
 */
void Split_String(const std::string &str, const char tokken, std::vector<std::string> &dest)
{
    std::string s;      // get's the split one at a time
    std::istringstream istr(str.c_str());

    while (std::getline(istr, s, tokken))
        dest.push_back(s);
} // end Split_String


//==============================================================================================================|
/**
 * @brief 
 *  Handles command-line arguments
 * 
 * @param [argv] array of strings containing command-line params according to the specs 
 * @param [argc] the argument count 
 * @param [filename] file name to override
 */
void Process_Command_Line(char **argv, const int argc, std::string &filename)
{
    for (int i{0}; i < argc; i++)
    {
        if (!strncmp("-d", argv[i], 2))
        {
            size_t len = strlen(argv[i]);

            for (size_t j{2}; j < len; j++)
                debug_mode |= (argv[i][j] - 0x30);
        } // end if debug
        

        if (!strncmp("-p", argv[i], strlen(argv[i])))
        {
            listen_port = atoi(argv[++i]);
        } // end if listen port override

        if (!strncmp("-fn", argv[i], strlen(argv[i])))
        {
            filename = argv[++i];
        } // end if file name

        if (!strncmp("-bs", argv[i], strlen(argv[i])))
        {
            buffer_size = atoi(argv[++i]);
        } // end if buffer size

        if (!strncmp("-bl", argv[i], strlen(argv[i])))
        {
            backlog = atoi(argv[++i]);
        } // end if backlog
    } // end for


    // while at it allocate memory for buffers
    if ( !(buffer = (char*)malloc(buffer_size)) )
    {
        perror("malloc fail");
        exit(EXIT_FAILURE);
    } // end if

    if ( !(snd_buffer = (char*)malloc(buffer_size + sizeof(INTAP_FMT))))
    {
        perror("malloc fail");
        exit(EXIT_FAILURE);
    } // end if
} // end Process_Command_Line


//==============================================================================================================|
//          THE END
//==============================================================================================================|