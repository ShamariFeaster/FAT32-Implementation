#ifndef _FEDIT_CONSTANTS_H_
#define _FEDIT_CONSTANTS_H_

#define FAT12 0
#define FAT16 1
#define FAT32 2

//FAT constants
#define Partition_LBA_Begin 0 //first byte of the partition
#define ENTRIES_PER_SECTOR 16
#define DIR_SIZE 32 //in bytes
#define SECTOR_SIZE 64//in bytes
#define FAT_ENTRY_SIZE 4//in bytes
#define MAX_FILENAME_SIZE 8
#define MAX_EXTENTION_SIZE 3
//file table
#define TBL_OPEN_FILES 50 //Max size of open file table
#define TBL_DIR_STATS 50  //Max size of open directory table
//open file codes
#define MODE_READ 0
#define MODE_WRITE 1
#define MODE_BOTH 2
#define MODE_UNKNOWN 3 //when first created and directories
//boolean support
#define TRUE 1
#define FALSE 0
typedef int bool;
//generic
#define SUCCESS 0
const char * ROOT = "/";
const char * PARENT = "..";
const char * SELF = ".";
const char * PATH_DELIMITER = "/";
//Parsing
#define PERIOD 46
#define ALPHA_NUMBERS_LOWER_BOUND 48
#define ALPHA_NUMBERS_UPPER_BOUND 57
#define ALPHA_UPPERCASE_LOWER_BOUND 64
#define ALPHA_UPPERCASE_UPPER_BOUND 90
#define ALPHA_LOWERCASE_LOWER_BOUND 94
#define ALPHA_LOWERCASE_UPPER_BOUND 122
//File attributes
const uint8_t ATTR_READ_ONLY = 0x01;
const uint8_t ATTR_HIDDEN = 0x02;
const uint8_t ATTR_SYSTEM = 0x04;
const uint8_t ATTR_VOLUME_ID = 0x08;
const uint8_t ATTR_DIRECTORY = 0x10;
const uint8_t ATTR_ARCHIVE = 0x20;
const uint8_t ATTR_LONG_NAME = 0x0F;
//Clusters   
uint32_t FAT_FREE_CLUSTER = 0x00000000;
uint32_t FAT_EOC = 0x0FFFFFF8;
uint32_t MASK_IGNORE_MSB = 0x0FFFFFFF;

#endif
