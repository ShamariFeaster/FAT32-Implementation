#ifndef _FEDIT_STRUCTURES_H_
#define _FEDIT_STRUCTURES_H_

/*
 * 
 * BEGIN STRUCTURE DEFINITIONS
 * 
 */ 
struct BS_BPB {
	uint8_t BS_jmpBoot[3]; //1-3
	uint8_t BS_OEMName[8]; //4-11
	uint16_t BPB_BytsPerSec;//12-13
	uint8_t BPB_SecPerClus; //14
	uint16_t BPB_RsvdSecCnt; //15-16
	uint8_t BPB_NumFATs; //17
	uint16_t BPB_RootEntCnt; //18-19
	uint16_t BPB_TotSec16; //20-21
	uint8_t BPB_Media; //22
	uint16_t BPB_FATSz16; //23-24
	uint16_t BPB_SecPerTrk; //25-26
	uint16_t BPB_NumHeads; //27-28
	uint32_t BPB_HiddSec; //29-32
	uint32_t BPB_TotSec32; //33-36
	uint32_t BPB_FATSz32 ; //37-40
	uint16_t BPB_ExtFlags ; //41-42
	uint16_t BPB_FSVer ; //43-44
	uint32_t BPB_RootClus ;//45-48
	uint16_t BPB_FSInfo ; //49-50
	uint16_t BPB_BkBootSec ;//51-52
	uint8_t BPB_Reserved[12]; //53-64
	uint8_t BS_DrvNum ;//65
	uint8_t BS_Reserved1 ;//66
	uint8_t BS_BootSig ;//67
	uint32_t BS_VolID ;//68-71
	uint8_t BS_VolLab[11]; //72-82
	uint8_t BS_FilSysType[8]; //83-89


} __attribute__((packed));

struct DIR_ENTRY {
    uint8_t filename[11];
    uint8_t attributes;
    uint8_t r1;
    uint8_t r2;
    uint16_t crtTime;
    uint16_t crtDate;
    uint16_t accessDate;
    uint8_t hiCluster[2];
    uint16_t lastWrTime;
    uint16_t lastWrDate;
    uint8_t loCluster[2];
    uint32_t fileSize;
} __attribute__((packed));

struct FILEDESCRIPTOR {
    uint8_t filename[9];
    uint8_t extention[4];
    char parent[100];
    uint32_t firstCluster;
    int mode;
    uint32_t size;
    bool dir; //is it a directory
    bool isOpen;
    uint8_t fullFilename[13];
} __attribute__((packed));

struct ENVIRONMENT {
    char pwd[100];
    char imageName[100];
    int pwd_cluster;
    uint32_t io_writeCluster;
    int tbl_dirStatsIndex;
    int tbl_openFilesIndex;
    int tbl_dirStatsSize;
    int tbl_openFilesSize;
    char last_pwd[100];
    struct FILEDESCRIPTOR * openFilesTbl; //open file history table
    struct FILEDESCRIPTOR * dirStatsTbl; //directory history table
    
} environment;

#endif
