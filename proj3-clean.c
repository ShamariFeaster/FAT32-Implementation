#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
#define TBL_OPEN_FILES 75 //Max size of open file table
#define TBL_DIR_STATS  75 //Max size of open directory table
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
const char * DEBUG_FLAG = "-d";
bool DEBUG;
//Parsing
#define PERIOD 46
#define ALPHA_NUMBERS_LOWER_BOUND 48
#define ALPHA_NUMBERS_UPPER_BOUND 57
#define ALPHA_UPPERCASE_LOWER_BOUND 65
#define ALPHA_UPPERCASE_UPPER_BOUND 90
#define ALPHA_LOWERCASE_LOWER_BOUND 97
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
    uint32_t io_writeCluster; //< - deprecated
    int tbl_dirStatsIndex;
    int tbl_openFilesIndex;
    int tbl_dirStatsSize;
    int tbl_openFilesSize;
    char last_pwd[100];
    struct FILEDESCRIPTOR * openFilesTbl; //open file history table
    struct FILEDESCRIPTOR * dirStatsTbl; //directory history table
    
} environment;

int checkForFileError(FILE * f) {
    if(f == NULL) {
        printf("FATAL ERROR: Problem openning image -- EXITTING!\n");
        exit(EXIT_FAILURE);
    }
} 

//-------------END BOOT SECTOR FUNCTIONS -----------------------
/* notes: the alias shown above the functions match up the variables from
 * the Microsoft FAT specificatiions
 */
  
//alias := rootDirSectors
int rootDirSectorCount(struct BS_BPB * bs) {
	return (bs->BPB_RootEntCnt * 32) + (bs->BPB_BytsPerSec - 1) / bs->BPB_BytsPerSec ;
	
}
//alias := FirstDataSector
int firstDataSector(struct BS_BPB * bs) {
	int FATSz;
	if(bs->BPB_FATSz16 != 0)
		FATSz = bs->BPB_FATSz16;
	else
		FATSz = bs->BPB_FATSz32;
	return bs->BPB_RsvdSecCnt + (bs->BPB_NumFATs * FATSz) + rootDirSectorCount(bs);

}
//alias := FirstSectorofCluster
uint32_t firstSectorofCluster(struct BS_BPB * bs, uint32_t clusterNum) {
	return (clusterNum - 2) * (bs->BPB_SecPerClus) + firstDataSector(bs);

}

/* decription: feed it a cluster and it returns the byte offset
 * of the beginning of that cluster's data
 * */
uint32_t byteOffsetOfCluster(struct BS_BPB *bs, uint32_t clusterNum) {
    return firstSectorofCluster(bs, clusterNum) * bs->BPB_BytsPerSec; 
}

//alias := DataSec
int sectorsInDataRegion(struct BS_BPB * bs) {
	int FATSz;
	int TotSec;
	if(bs->BPB_FATSz16 != 0)
		FATSz = bs->BPB_FATSz16;
	else
		FATSz = bs->BPB_FATSz32;
	if(bs->BPB_TotSec16 != 0)
		TotSec = bs->BPB_TotSec16;
	else
		TotSec = bs->BPB_TotSec32;
	return TotSec - (bs->BPB_RsvdSecCnt + (bs->BPB_NumFATs * FATSz) + rootDirSectorCount(bs));

}

//alias := CountofClusters
int countOfClusters(struct BS_BPB * bs) {
	return sectorsInDataRegion(bs) / bs->BPB_SecPerClus;
}

int getFatType(struct BS_BPB * bs) {
	int clusterCount =  countOfClusters(bs);
	if(clusterCount < 4085)
		return FAT12;
	else if(clusterCount < 65525)	
		return FAT16;
	else
		return FAT32;
}

int firstFatTableSector(struct BS_BPB *bs) {
    return Partition_LBA_Begin + bs->BPB_RsvdSecCnt;
}
//the sector of the first clustor
//cluster_begin_lba
int firstClusterSector(struct BS_BPB *bs) {
    return Partition_LBA_Begin + bs->BPB_RsvdSecCnt + (bs->BPB_NumFATs * bs->BPB_SecPerClus);
}

 
//----------------OPEN FILE/DIRECTORY TABLES-----------------
/* create a new file to be put in the file descriptor table
 * */
struct FILEDESCRIPTOR * TBL_createFile(
                                const char * filename, 
                                const char * extention, 
                                const char * parent, 
                                uint32_t firstCluster, 
                                int mode, 
                                uint32_t size, 
                                int dir,
								int isOpen) {
    struct FILEDESCRIPTOR * newFile = (struct FILEDESCRIPTOR *) malloc(sizeof(struct FILEDESCRIPTOR));
    strcpy(newFile->filename, filename);
    strcpy(newFile->extention, extention);
    strcpy(newFile->parent, parent);
    
    if(strlen(newFile->extention) > 0) {
        strcpy(newFile->fullFilename, newFile->filename);
        strcat(newFile->fullFilename, ".");
        strcat(newFile->fullFilename, newFile->extention);
    } else {
        strcpy(newFile->fullFilename, newFile->filename);
    }
    
    newFile->firstCluster = firstCluster;
    newFile->mode = mode;
    newFile->size = size;
    newFile->dir = dir;
	newFile->isOpen = isOpen;
    return newFile;
}
/* adds a file object to either the fd table or the dir history table
 */ 
int TBL_addFileToTbl(struct FILEDESCRIPTOR * file, int isDir) {
    if(isDir == TRUE) {
        if(environment.tbl_dirStatsSize < TBL_DIR_STATS) { 
            environment.dirStatsTbl[environment.tbl_dirStatsIndex % TBL_DIR_STATS] = *file;
			if(DEBUG == TRUE) printf("adding %s\n", environment.dirStatsTbl[environment.tbl_dirStatsIndex % TBL_DIR_STATS].filename);
            environment.tbl_dirStatsSize++;
			environment.tbl_dirStatsIndex = ++environment.tbl_dirStatsIndex % TBL_DIR_STATS;
			return 0;
        }else {
			environment.dirStatsTbl[environment.tbl_dirStatsIndex % TBL_DIR_STATS] = *file;
			environment.tbl_dirStatsIndex = ++environment.tbl_dirStatsIndex % TBL_DIR_STATS;
			return 0;
		}		
        
        
    } else {
        if(environment.tbl_openFilesSize < TBL_OPEN_FILES) {
            environment.openFilesTbl[environment.tbl_openFilesIndex % TBL_OPEN_FILES] = *file;
            environment.tbl_openFilesSize++;
			environment.tbl_openFilesIndex = ++environment.tbl_openFilesIndex % TBL_OPEN_FILES;
			return 0;
        } else {
            environment.openFilesTbl[environment.tbl_openFilesIndex % TBL_OPEN_FILES] = *file;
			environment.tbl_openFilesIndex = ++environment.tbl_openFilesIndex % TBL_OPEN_FILES;
			return 0;
        }
        
    }
}

/* description: if <isDir> is set prints the content of the open files table
 * if not prints the open directory table
 */ 
int TBL_printFileTbl(int isDir) {
	if(DEBUG == TRUE) printf("environment.tbl_dirStatsSize: %d\n", environment.tbl_dirStatsSize);
	if(DEBUG == TRUE) printf("environment.tbl_openFilesSize: %d\n", environment.tbl_openFilesSize);
	if(isDir == TRUE) {
		puts("\nDirectory Table\n");
		if(environment.tbl_dirStatsSize > 0) {
			int x;
			for(x = 0; x < environment.tbl_dirStatsSize; x++) {
				
				printf("[%d] filename: %s, parent: %s, isOpen: %d, Size: %d, mode: %d\n", 
					x,environment.dirStatsTbl[x % TBL_DIR_STATS].fullFilename,
					environment.dirStatsTbl[x % TBL_DIR_STATS].parent,
					environment.dirStatsTbl[x % TBL_DIR_STATS].isOpen,
					environment.dirStatsTbl[x % TBL_DIR_STATS].size,
					environment.dirStatsTbl[x % TBL_DIR_STATS].mode);
			}
			return 0;
		} else {
			return 1;
		}
	} else {
		puts("\nOpen File Table\n");
		if(environment.tbl_openFilesSize > 0) {
			int x;
			for(x = 0; x < environment.tbl_openFilesSize; x++) {
			
				printf("[%d] filename: %s, parent:%s, isOpen: %d, Size: %d, mode: %d\n", 
					x,environment.openFilesTbl[x % TBL_OPEN_FILES].fullFilename,
					environment.openFilesTbl[x % TBL_OPEN_FILES].parent,
					environment.openFilesTbl[x % TBL_OPEN_FILES].isOpen,
					environment.openFilesTbl[x % TBL_OPEN_FILES].size,
					environment.openFilesTbl[x % TBL_OPEN_FILES].mode);
					
			} 
			return 0;
		} else {
				return 1;
			}
	}
}

/* desctipion searches the open directory table for entry if it finds it,
 * it returns the name of the parent directory, else it returns an empty string
*/
const char * TBL_getParent(const char * dirName) {
	if(environment.tbl_dirStatsSize > 0) {
			int x;
			for(x = 0; x < environment.tbl_dirStatsSize; x++) {
			if(DEBUG == TRUE) printf("searching for %s in table, found %s\n", dirName, environment.dirStatsTbl[x % TBL_DIR_STATS].filename);
					if(strcmp(environment.dirStatsTbl[x % TBL_DIR_STATS].filename, dirName) == 0)
						return environment.dirStatsTbl[x % TBL_DIR_STATS].parent;
			}
			return "";
		} else {
			return "";
		}
}

/* description:  "index" is set to the index where the found element resides
 * returns TRUE if file was found in the file table
 * 
 */ 
bool TBL_getFileDescriptor(int * index, const char * filename, bool isDir){
    struct FILEDESCRIPTOR  * b;
    if(isDir == TRUE) {
        if(environment.tbl_dirStatsSize > 0) {

			int x;
			for(x = 0; x < environment.tbl_dirStatsSize; x++) {
			if(DEBUG == TRUE) printf("searching for %s in table, found %s\n", filename, environment.dirStatsTbl[x % TBL_DIR_STATS].filename);
					if(strcmp(environment.dirStatsTbl[x % TBL_DIR_STATS].fullFilename, filename) == 0) {
                        *index = x;
                        return TRUE;
                    }
                }
            
        } else
            return FALSE;
    } else {

        if(environment.tbl_openFilesSize > 0) {

			int x;
			for(x = 0; x < environment.tbl_openFilesSize; x++) {
              
					if(strcmp(environment.openFilesTbl[x % TBL_OPEN_FILES].fullFilename, filename) == 0) {
                        *index = x;
                        return TRUE;
                    }
                }
            
        } else
            return FALSE;
    }

    return FALSE;
}
//-----------------------------------------------------------------------------
/*description: feed this a cluster and it returns an offset, in bytes, where the
 * fat entry for this cluster is locted
 * 
 * use: use the result to seek to the place in the image and read
 * the FAT entry
* */
uint32_t getFatAddressByCluster(struct BS_BPB * bs, uint32_t clusterNum) {
    uint32_t FATOffset = clusterNum * 4;
    uint32_t ThisFATSecNum = bs->BPB_RsvdSecCnt + (FATOffset / bs->BPB_BytsPerSec);
    uint32_t ThisFATEntOffset = FATOffset % bs->BPB_BytsPerSec;
    return (ThisFATSecNum * bs->BPB_BytsPerSec + ThisFATEntOffset); 
}

/*description: finds the location of this clusters FAT entry, reads
 * and returns the entry value
 * 
 * use: use the result to seek to the place in the image and read
 * the FAT entry
* */
uint32_t FAT_getFatEntry(struct BS_BPB * bs, uint32_t clusterNum) {
    
    FILE* f = fopen(environment.imageName, "r");
    checkForFileError(f);
    uint8_t aFatEntry[FAT_ENTRY_SIZE];
    uint32_t FATOffset = clusterNum * 4;
    fseek(f, getFatAddressByCluster(bs, clusterNum), 0);
    fread(aFatEntry, 1, FAT_ENTRY_SIZE, f);
    fclose(f);
    uint32_t fatEntry = 0x00000000;

    int x;
    for(x = 0; x < 4; x++) {
        fatEntry |= aFatEntry[(FATOffset % FAT_ENTRY_SIZE ) + x] << 8 * x;
    }

    return fatEntry;
}
/* description: takes a value, newFatVal, and writes it to the destination
 * cluster, 
 */ 
int FAT_writeFatEntry(struct BS_BPB * bs, uint32_t destinationCluster, uint32_t * newFatVal) {
    FILE* f = fopen(environment.imageName, "r+");
    checkForFileError(f);
    fseek(f, getFatAddressByCluster(bs, destinationCluster), 0);
    fwrite(newFatVal, 4, 1, f);
    fclose(f);
    if(DEBUG == TRUE) printf("FAT_writeEntry: wrote->%d to cluster %d", *newFatVal, destinationCluster);
    return 0;
}

/* desctipion: NOT USED but might be in future to clear newly freed clusters
*/
void clearCluster(struct BS_BPB * bs, uint32_t targetCluster) {
	uint8_t clearedCluster[bs->BPB_BytsPerSec];
	//i didn't want to do it this way but compiler wont let me initalize 
	//an array that's sized with a variable :(
	int x;	
	for(x = 0; x < bs->BPB_BytsPerSec; x++) { clearedCluster[x] = 0x00; }
	FILE* f = fopen(environment.imageName, "r+");
    checkForFileError(f);
    fseek(f, byteOffsetOfCluster(bs, targetCluster), 0);
	fwrite(clearedCluster, 1, bs->BPB_BytsPerSec, f);
	fclose(f);
}

/*description:walks a FAT chain until 0x0fffffff, returns the number of iterations
 * which is the size of the cluster chiain
*/ 
int getFileSizeInClusters(struct BS_BPB * bs, uint32_t firstClusterVal) {
    int size = 1;
    firstClusterVal = (int) FAT_getFatEntry(bs, firstClusterVal);

    while((firstClusterVal = (firstClusterVal & MASK_IGNORE_MSB)) < FAT_EOC) {
       
        size++;
        firstClusterVal = FAT_getFatEntry(bs, firstClusterVal);
    }
    return size;
        
}
/*description: walks a FAT chain until returns the last cluster, where
* the current EOC is. returns the cluster number passed in if it's empty,
* , you should probly call getFirstFreeCluster if you intend to write to
* the FAT to keep things orderly
*/ 
uint32_t getLastClusterInChain(struct BS_BPB * bs, uint32_t firstClusterVal) {
    int size = 1;
    uint32_t lastCluster = firstClusterVal;
    firstClusterVal = (int) FAT_getFatEntry(bs, firstClusterVal);
    //if cluster is empty return cluster number passed in
    if((((firstClusterVal & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER) )
        return lastCluster;
    //mask the 1st 4 msb, they are special and don't count    
    while((firstClusterVal = (firstClusterVal & MASK_IGNORE_MSB)) < FAT_EOC) {
        lastCluster = firstClusterVal;
        firstClusterVal = FAT_getFatEntry(bs, firstClusterVal);
    }
    return lastCluster;
        
}
/* description: traverses the FAT sequentially and find the first open cluster
 */ 
int FAT_findFirstFreeCluster(struct BS_BPB * bs) {
    int i = 0;
    int totalClusters = countOfClusters(bs);
    while(i < totalClusters) {
        if(((FAT_getFatEntry(bs, i) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER)
            break;
        i++;
    }
    return i;
}
/* decription: returns the total number of open clusters
 */ 
int FAT_findTotalFreeCluster(struct BS_BPB * bs) {
    int i = 0;
    int fatIndex = 0;
    int totalClusters = countOfClusters(bs);
    while(fatIndex < totalClusters) {
        if((((FAT_getFatEntry(bs, fatIndex) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER))
            i++;
        fatIndex++;
    }
    return i;
}



/* description: pass in a entry and this properly formats the
 * "firstCluster" from the 2 byte segments in the file structure
 */
uint32_t buildClusterAddress(struct DIR_ENTRY * entry) {
    uint32_t firstCluster = 0x00000000;
    firstCluster |=  entry->hiCluster[1] << 24;
    firstCluster |=  entry->hiCluster[0] << 16;
    firstCluster |=  entry->loCluster[1] << 8;
    firstCluster |=  entry->loCluster[0];
    return firstCluster;
}

/* description: convert uint32_t to hiCluster and loCluster byte array
 * and stores it into <entry>
 */ 
int deconstructClusterAddress(struct DIR_ENTRY * entry, uint32_t cluster) {
    entry->loCluster[0] = cluster;
    entry->loCluster[1] = cluster >> 8;
    entry->hiCluster[0] = cluster >> 16;
    entry->hiCluster[1] = cluster >> 24;
    return 0;
}
/* description: takes a directory entry populates a file descriptor 
 * to be used in the file tables
 * */
struct FILEDESCRIPTOR * makeFileDecriptor(struct DIR_ENTRY * entry, struct FILEDESCRIPTOR * fd) {
    char newFilename[12];
    bzero(fd->filename, 9);
    bzero(fd->extention, 4);
    memcpy(newFilename, entry->filename, 11);
    newFilename[11] = '\0';
    int x;
    for(x = 0; x < 8; x++) {
        if(newFilename[x] == ' ')
            break;
        fd->filename[x] = newFilename[x];
    }
    fd->filename[++x] = '\0';
    for(x = 8; x < 11; x++) {
        if(newFilename[x] == ' ')
            break;
        fd->extention[x - 8] = newFilename[x];
    }
    fd->extention[++x - 8] = '\0';
    if(strlen(fd->extention) > 0) {
        strcpy(fd->fullFilename, fd->filename);
        strcat(fd->fullFilename, ".");
        strcat(fd->fullFilename, fd->extention);
    } else {
        strcpy(fd->fullFilename, fd->filename);
    }
    fd->firstCluster = buildClusterAddress(entry);
	fd->size = entry->fileSize;
	fd->mode = MODE_UNKNOWN;
    if((entry->attributes & ATTR_DIRECTORY) == ATTR_DIRECTORY)
        fd->dir = TRUE;
    else
        fd->dir = FALSE;
    return fd;
}

/* description: prints the contents of a directory entry
*/
int showEntry(struct DIR_ENTRY * entry) {
	puts("First Cluster\n");
	printf("lo[0]%02x, lo[1]%02x, hi[0]%02x, hi[1]%02x\n", entry->loCluster[0],
															entry->loCluster[1],
															entry->hiCluster[0],
															entry->hiCluster[1]);
	int x;
	for(x = 0; x < 11; x++) {
	if(entry->filename[x] == ' ')
		printf("filename[%d]->%s\n", x, "SPACE");
	else
		printf("filename[%d]->%c\n", x, entry->filename[x]);
	}

	printf("\nattr->0x%x, size->0x%x ", entry->attributes, entry->fileSize);	


}

/* description: takes a cluster number where bunch of directories are and
 * the offst of the directory you want read and it will store that directory
 * info into the variable dir
 */ 
struct DIR_ENTRY * readEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t clusterNum, int offset){
    offset *= 32;
    uint32_t dataAddress = byteOffsetOfCluster(bs, clusterNum);
    
    FILE* f = fopen(environment.imageName, "r");
    checkForFileError(f);
    fseek(f, dataAddress + offset, 0);
	fread(entry, sizeof(struct DIR_ENTRY), 1, f);
    
    fclose(f);
    return entry;
}
/* description: takes a cluster number and offset where you want an entry written and write to that 
 * position
 */ 
struct DIR_ENTRY * writeEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t clusterNum, int offset){
    offset *= 32;
    uint32_t dataAddress = byteOffsetOfCluster(bs, clusterNum);
    FILE* f = fopen(environment.imageName, "r+");
    checkForFileError(f);

    fseek(f, dataAddress + offset, 0);
	fwrite(entry, 1, sizeof(struct DIR_ENTRY), f);
    if(DEBUG == TRUE) printf("writeEntry-> address: %d...Calling showEntry()\n", dataAddress);
    if(DEBUG == TRUE) showEntry(entry);
    fclose(f);
    return entry;
}
/* description: takes a directory cluster and give you the byte address of the entry at
 * the offset provided. used to write dot entries. POSSIBLY REDUNDANT
 */ 
uint32_t byteOffsetofDirectoryEntry(struct BS_BPB * bs, uint32_t clusterNum, int offset) {
    if(DEBUG == TRUE) printf("\nbyteOffsetofDirectoryEntry: passed in offset->%d\n", offset);
    offset *= 32;
    if(DEBUG == TRUE) printf("\nbyteOffsetofDirectoryEntry: offset*32->%d\n", offset);
    uint32_t dataAddress = byteOffsetOfCluster(bs, clusterNum);
    if(DEBUG == TRUE) printf("\nbyteOffsetofDirectoryEntry: clusterNum: %d, offset: %d, returning: %d\n", clusterNum, offset, (dataAddress + offset));
    return (dataAddress + offset);
}

/* This is the workhorse. It is used for ls, cd, filesearching. 
 * Directory Functionality:
 * It is used to perform cd, which you use by setting <cd>. if <goingUp> is set it uses 
 * the open directory table to find the pwd's parent. if not it searches the pwd for
 * <directoryName>. If <cd> is not set this prits the contents of the pwd to the screen
 * 
 * Search Functionality
 * if <useAsSearch> is set this short circuits the cd functionality and returns TRUE
 * if found
 * 
 * ls(bs, environment.pwd_cluster, TRUE, fileName, FALSE, searchFile, TRUE, searchForDirectory) 
 */ 

int ls(struct BS_BPB * bs, uint32_t clusterNum, bool cd, const char * directoryName, bool goingUp, struct FILEDESCRIPTOR * searchFile, bool useAsSearch, bool searchForDir) {

    struct DIR_ENTRY dir;
	int dirSizeInCluster = getFileSizeInClusters(bs, clusterNum);
    int clusterCount;
    char fileName[13];
    char f[12];
    int offset;
    int increment;
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {
        //root doesn't have . and .., so we traverse it differently
        // we only increment 1 to get . and .. then we go in 2's to avoid longname entries
        if(strcmp(directoryName, ROOT) == 0) {
            offset = 1;
            increment = 2;
        } else {
            offset = 0;
            increment = 1;
        }
		
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
            //this is a hack to properly display the dir contents of folders
            //with . and .. w/o displaying longname entries
            //if not root folder we need to read the 1st 2 entries (. and ..)
            //sequential then resume skipping longnames by incrementing by 2
            if(strcmp(directoryName, ROOT) != 0 && offset == 2) {
                increment = 2;
                offset -= 1;			
                continue;
            }
			
            readEntry(bs, &dir, clusterNum, offset);
            makeFileDecriptor(&dir, searchFile);
            
            if( cd == FALSE ) {
                //we don't want to print 0xE5 to the screen
                if(searchFile->filename[0] != 0xE5) {
                    if(searchFile->dir == TRUE) 
                        printf("dir->%s   ", searchFile->fullFilename);
                    else
                        printf("%s     ", searchFile->fullFilename);
                }
            } else {
                
                //if there's an extention we expect the user must type it in with the '.' and the extention
                //because of this we must compare it to a recontructed filename with the implied '.' put
                //back in. also searchForDir tells us whether the file being searched for is a directory or not
                if(useAsSearch == TRUE) {
                     if(strcmp(searchFile->fullFilename, directoryName) == 0 && searchFile->dir == searchForDir ) 
                        return TRUE;
                } else {   
                
                    /* if we're going down we search pwd for directory with the name passed in.
                    * if we find it we add it to the directory table and return TRUE. else
                    * we fall out of this loop and return FALSE at the end of the function
                    */
                    if(searchFile->dir == TRUE && strcmp(searchFile->fullFilename, directoryName) == 0 && goingUp == FALSE) {// <--------CHANGED
                        environment.pwd_cluster = searchFile->firstCluster;
                        if(strcmp(TBL_getParent(directoryName), "") == 0)//if directory not already in open dir history table
                            TBL_addFileToTbl(TBL_createFile(searchFile->filename, "", environment.pwd, searchFile->firstCluster, searchFile->mode, searchFile->size, TRUE, FALSE), TRUE);

                        FAT_setIoWriteCluster(bs, environment.pwd_cluster);
                        return TRUE;
                    }
                    
                    //fetch parent cluster from the directory table
                    if(searchFile->dir == TRUE && strcmp(directoryName, PARENT) == 0 && goingUp == TRUE) {
                        const char * parent = TBL_getParent(environment.pwd);
                        if(strcmp(parent, "") != 0) { //we found parent in the table
                        
                            if(strcmp(parent, "/") == 0) 
                                environment.pwd_cluster = 2; //sets pwd_cluster to where we're headed
                            else 
                                environment.pwd_cluster = searchFile->firstCluster; 
                            
                            strcpy(environment.last_pwd, environment.pwd);
                            strcpy(environment.pwd, TBL_getParent(environment.pwd));  
                            FAT_setIoWriteCluster(bs, environment.pwd_cluster);
                            
                            return TRUE; //cd to parent was successful
                        } else 
                            return FALSE; //cd to parent not successful
                    }
                    
                } // end cd block

            }
       
        }
        
        clusterNum = FAT_getFatEntry(bs, clusterNum);
        //printf("next cluster: %d\n", FAT_getFatEntry(bs, clusterNum));
    
    }
    return FALSE;
}

/* descriptioin: takes a FILEDESCRIPTOR and checks if the entry it was
 * created from is empty. Helper function
 */ 
bool isEntryEmpty(struct FILEDESCRIPTOR * fd) {
    if((fd->filename[0] != 0x00) && (fd->filename[0] != 0xE5) )
        return FALSE;
    else
        return TRUE;
}

/* description: walks a directory cluster chain looking for empty entries, if it finds one
 * it returns the byte offset of that entry. If it finds none then it returns -1;
 */ 
uint32_t FAT_findNextOpenEntry(struct BS_BPB * bs, uint32_t pwdCluster) {

    struct DIR_ENTRY dir;
    struct FILEDESCRIPTOR fd;
    int dirSizeInCluster = getFileSizeInClusters(bs, pwdCluster);
    int clusterCount;
    char fileName[12];
    int offset;
    int increment;
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {
        if(strcmp(environment.pwd, ROOT) == 0) {
            offset = 1;
            increment = 2;
        } else {
            offset = 0;
            increment = 1;
        }
		
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
            if(strcmp(environment.pwd, ROOT) != 0 && offset == 2) {
                increment = 2;
                offset -= 1;	
                continue;
            }

            if(DEBUG == TRUE) printf("\nFAT_findNextOpenEntry: offset->%d\n", offset); 

            readEntry(bs, &dir, pwdCluster, offset);
            makeFileDecriptor(&dir, &fd);

            if( isEntryEmpty(&fd) == TRUE ) {
                //this should tell me exactly where to write my new entry to
                //printf("cluster #%d, byte offset: %d: ", offset, byteOffsetofDirectoryEntry(bs, pwdCluster, offset));             
                return byteOffsetofDirectoryEntry(bs, pwdCluster, offset);
            }
        }
        //pwdCluster becomes the next cluster in the chain starting at the passed in pwdCluster
       pwdCluster = FAT_getFatEntry(bs, pwdCluster); 
      
    }
    return -1; //cluster chain is full
}

/* description: wrapper for ls. tries to change the pwd to <directoryName> 
 * This doesn't use the print to screen functinoality of ls. if <goingUp> 
 * is set it uses the open directory table to find the pwd's parent
 */ 
int cd(struct BS_BPB * bs, const char * directoryName, int goingUp,  struct FILEDESCRIPTOR * searchFile) {
   
    return ls(bs, environment.pwd_cluster, TRUE, directoryName, goingUp, searchFile, FALSE, FALSE); 
}

/* desctipion: wrapper that uses ls to search a pwd for a file and puts its info
 * into searchFile if found. if <useAsFileSearch> is set it disables the printing enabling
 * it to be used as a file search utility. if <searchForDirectory> is set it excludes
 * files from the search
 */ 
bool searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile) {
    
    if(ls(bs, environment.pwd_cluster, TRUE, fileName, FALSE, searchFile, TRUE, searchForDirectory) == TRUE) {
        if(useAsFileSearch == FALSE)
            printf("File: %s is %d byte(s) in size", fileName ,searchFile->size);
        return TRUE;
    } else {
        if(useAsFileSearch == FALSE)
            printf("ERROR: File: %s was not found", fileName);
        return FALSE;
    }
}

/* desctipion: wrapper that uses ls to search a pwd for a file and puts its info
 * into searchFile if found. Prints result for both file and directories
 */ 
bool printFileOrDirectorySize(struct BS_BPB * bs, const char * fileName, struct FILEDESCRIPTOR * searchFile) {
    
    if(ls(bs, environment.pwd_cluster, TRUE, fileName, FALSE, searchFile, TRUE, TRUE) == TRUE ||
    ls(bs, environment.pwd_cluster, TRUE, fileName, FALSE, searchFile, TRUE, FALSE) == TRUE) {
        printf("File: %s is %d byte(s) in size", fileName ,searchFile->size);
        return TRUE;
    } else 
        return FALSE;
    
}

/* description: wrapper for searchOrPrintFileSize searchs pwd for filename. 
 * returns TRUE if found, if found the resulting entry is put into searchFile
 * for use outside the function. if <searchForDirectory> is set it excludes
 * files from the search
 */ 
bool searchForFile(struct BS_BPB * bs, const char * fileName, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile) {
    return searchOrPrintFileSize(bs, fileName, TRUE, searchForDirectory, searchFile);
} 

/* decription: feed this a cluster number that's part of a chain and this
 * traverses it to find the last entry as well as finding the first available
 * free cluster and adds that free cluster to the chain
 */ 
uint32_t FAT_extendClusterChain(struct BS_BPB * bs,  uint32_t clusterChainMember) {
    uint32_t firstFreeCluster = FAT_findFirstFreeCluster(bs);
	uint32_t lastClusterinChain = getLastClusterInChain(bs, clusterChainMember);

    FAT_writeFatEntry(bs, lastClusterinChain, &firstFreeCluster);
    FAT_writeFatEntry(bs, firstFreeCluster, &FAT_EOC);
    return firstFreeCluster;
}

/* desctipion: pass in a free cluster and this marks it with EOC and sets 
 * the environment.io_writeCluster to the end of newly created cluster 
 * chain
*/
int FAT_allocateClusterChain(struct BS_BPB * bs,  uint32_t clusterNum) {
	FAT_writeFatEntry(bs, clusterNum, &FAT_EOC);
	return 0;
}

/* desctipion: pass in the 1st cluster of a chain and this traverses the 
 * chain and writes FAT_FREE_CLUSTER to every link, thereby freeing the 
 * chain for reallocation
*/
int FAT_freeClusterChain(struct BS_BPB * bs,  uint32_t firstClusterOfChain){
    int dirSizeInCluster = getFileSizeInClusters(bs, firstClusterOfChain);
    if(DEBUG == TRUE) printf("dir Size: %d\n", dirSizeInCluster);
    int currentCluster= firstClusterOfChain;
    int nextCluster;
    int clusterCount;
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {  
       nextCluster = FAT_getFatEntry(bs, currentCluster); 
       FAT_writeFatEntry(bs, currentCluster, &FAT_FREE_CLUSTER);
       currentCluster = nextCluster;
    }
    return 0; 
    
    }

/* description: feed this a cluster that's a member of a chain and this
 * sets your environment variable "io_writeCluster" to the last in that
 * chain. The thinking is this is where you'll write in order to grow
 * that file. before writing to cluster check with FAT_findNextOpenEntry()
 * to see if there's space or you need to extend the chain using FAT_extendClusterChain()
 */ 
int FAT_setIoWriteCluster(struct BS_BPB * bs, uint32_t clusterChainMember) {
     environment.io_writeCluster = getLastClusterInChain(bs, clusterChainMember);
	 return 0;
}

/* description: this will write a new entry to <destinationCluster>. if that cluster
 * is full it grows the cluster chain and write the entry in the first spot
 * of the new cluster. if <isDotEntries> is set this automatically writes both
 * '.' and '..' to offsets 0 and 1 of <destinationCluster>
 * 
 */ 
int writeFileEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t destinationCluster, bool isDotEntries) {
    int dataAddress;
    int freshCluster;
    FILE* f = fopen(environment.imageName, "r+");
    checkForFileError(f);
    if(isDotEntries == FALSE) {
        if((dataAddress = FAT_findNextOpenEntry(bs, destinationCluster)) != -1) {//-1 means current cluster is at capacity
            fseek(f, dataAddress, 0);
            fwrite (entry , 1 , sizeof(struct DIR_ENTRY) , f );
        } else {
            freshCluster = FAT_extendClusterChain(bs, destinationCluster);
            dataAddress = FAT_findNextOpenEntry(bs, freshCluster);
            fseek(f, dataAddress, 0);
            fwrite (entry , 1 , sizeof(struct DIR_ENTRY) , f );
        }
    } else {
        struct DIR_ENTRY dotEntry;
        struct DIR_ENTRY dotDotEntry;
        makeSpecialDirEntries(&dotEntry, &dotDotEntry, destinationCluster, environment.pwd_cluster);
        //seek to first spot in new dir cluster chin and write the '.' entry
        dataAddress = byteOffsetofDirectoryEntry(bs, destinationCluster, 0);
        fseek(f, dataAddress, 0);
        fwrite (&dotEntry , 1 , sizeof(struct DIR_ENTRY) , f );
        //seek to second spot in new dir cluster chin and write the '..' entry
        dataAddress = byteOffsetofDirectoryEntry(bs, destinationCluster, 1);
        fseek(f, dataAddress, 0);
        fwrite (&dotDotEntry , 1 , sizeof(struct DIR_ENTRY) , f );
    }
     fclose(f);
     return 0;
}



/* description: takes a directory entry and all the necesary info
	and populates the entry with the info in a correct format for
	insertion into a disk.
*/
int createEntry(struct DIR_ENTRY * entry,
			const char * filename, 
			const char * ext,
			int isDir,
			uint32_t firstCluster,
			uint32_t filesize,
            bool emptyAfterThis,
            bool emptyDirectory) {
    //set the same no matter the entry
    entry->r1 = 0;
	entry->r2 = 0;
	entry->crtTime = 0;
	entry->crtDate = 0;
	entry->accessDate = 0;
	entry->lastWrTime = 0;
	entry->lastWrDate = 0;

    if(emptyAfterThis == FALSE && emptyDirectory == FALSE) { //if both are false
        int x;
        for(x = 0; x < MAX_FILENAME_SIZE; x++) {
            if(x < strlen(filename))
                entry->filename[x] = filename[x];
            else
                entry->filename[x] = ' ';
        }

        for(x = 0; x < MAX_EXTENTION_SIZE; x++) {
            if(x < strlen(ext))
                entry->filename[MAX_FILENAME_SIZE + x] = ext[x];
            else
                entry->filename[MAX_FILENAME_SIZE + x] = ' ';
        }
        
        deconstructClusterAddress(entry, firstCluster);

        if(isDir == TRUE) {
            entry->fileSize = 0;
            entry->attributes = ATTR_DIRECTORY;
		} else {
            entry->fileSize = filesize;
            entry->attributes = ATTR_ARCHIVE;
		}
        return 0; //stops execution so we don't flow out into empty entry config code below
        
    } else if(emptyAfterThis == TRUE) { //if this isn't true, then the other must be
        entry->filename[0] = 0xE5;
        entry->attributes = 0x00;
    }else {                             //hence no condition check here
        entry->filename[0] = 0x00;
        entry->attributes = 0x00;
    }
    
    //if i made it here we're creating an empty entry and both conditions
    //require this setup
    int x;
    for(x = 1; x < 11; x++) 
        entry->filename[x] = 0x00;
    
	entry->loCluster[0] = 0x00;
    entry->loCluster[1] = 0x00;
    entry->hiCluster[0] = 0x00;
    entry->hiCluster[1] = 0x00;
	entry->attributes = 0x00;
	entry->fileSize = 0;
	
    return 0;
}

/* description: take two entries and cluster info and populates	them with 
 * '.' and '..' entriy information. The entries are	ready for writing to 
 * the disk after this. helper function for mkdir()
  */
int makeSpecialDirEntries(struct DIR_ENTRY * dot, 
						struct DIR_ENTRY * dotDot,
						uint32_t newlyAllocatedCluster,
						uint32_t pwdCluster ) {
	createEntry(dot, ".", "", TRUE, newlyAllocatedCluster, 0, FALSE, FALSE);	
	createEntry(dotDot, "..", "", TRUE, pwdCluster, 0, FALSE, FALSE);	
	return 0;
}

/* returns offset of the entry, <searchName> in the pwd 
 * if found return the offset, if not return -1 
 */ 
int getEntryOffset(struct BS_BPB * bs, const char * searchName) {
    struct DIR_ENTRY entry;
    struct FILEDESCRIPTOR fd;
    uint32_t currentCluster = environment.pwd_cluster;
    int dirSizeInCluster = getFileSizeInClusters(bs, currentCluster);
    int clusterCount;
    int offset;
    int increment;

    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {
        if(strcmp(environment.pwd, ROOT) == 0) {
            offset = 1;
            increment = 2;
        } else {
            offset = 0;
            increment = 1;
        }
		
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
            if(strcmp(environment.pwd, ROOT) != 0 && offset == 2) {
                increment = 2;
                offset -= 1;	
                continue;
            }
			
            readEntry(bs, &entry, currentCluster, offset);
            makeFileDecriptor(&entry, &fd);

            if(strcmp(searchName, fd.fullFilename) == 0) {
            
                return offset;
            }
                
       
      
        }
        currentCluster = FAT_getFatEntry(bs, currentCluster); 
   
    }
     return -1;
}
/* description: prints info about the image to the screen
 */ 
int showDriveDetails(struct BS_BPB * bs) {

     puts("\nBoot Sector Info:\n");
    if(DEBUG == TRUE) {
        printf("First jmpBoot value (must be 0xEB or 0xE9): 0x%x\n", bs->BS_jmpBoot[0]);
        printf("OEM Name: %s\n", bs->BS_OEMName);
        printf("Sectors occupied by one FAT: %d\n", bs->BPB_FATSz32);
        printf("Number of  Root Dirs: %d\n", bs->BPB_RootEntCnt);
        printf("Total Number of Sectors: %d\n", bs->BPB_TotSec32);
        printf("Fat Type: %d\n\n", getFatType(bs));
        printf("Data Sector size (kB): %d\n", (sectorsInDataRegion(bs) * 512) / 1024);
        printf("Number of Reserved Sectors: %d\n", bs->BPB_RsvdSecCnt);
        puts("More Info:\n");
        printf("Number of Data sectors: %d\n", sectorsInDataRegion(bs));
        printf("Root Dir 1st Cluster: %d\n", bs->BPB_RootClus);
        printf("1st data sector: %d\n", firstDataSector(bs));
        printf("1st Sector of the Root Dir: %d\n\n ", firstSectorofCluster(bs, bs->BPB_RootClus));
    }
	printf("Bytes Per Sector (block size):%d\n", bs->BPB_BytsPerSec);
	printf("Sectors per Cluster: %d\n", bs->BPB_SecPerClus);
    printf("Total clusters: %d\n", countOfClusters(bs));
	printf("Number of FATs: %d\n", bs->BPB_NumFATs);
    printf("Sectors occupied by one FAT: %d\nComputing Free Sectors.......\n", bs->BPB_FATSz32);
    printf("Number of free sectors: %d\n", FAT_findTotalFreeCluster(bs));

    
    
    return 0;
}
/* description: DOES NOT allocate a cluster chain. Writes a directory 
 * entry to <targetDirectoryCluster>. Error handling is done in the driver
 */ 
int touch(struct BS_BPB * bs, const char * filename, const char * extention, uint32_t targetDirectoryCluster) {
    struct DIR_ENTRY newFileEntry;
    createEntry(&newFileEntry, filename, extention, FALSE, 0, 0, FALSE, FALSE);
    writeFileEntry(bs, &newFileEntry, targetDirectoryCluster, FALSE);
    return 0;
    }
/* description: allocates a new cluster chain to <dirName> and writes the directory
 * entry to <targetDirectoryCluster>, then writes '.' and '..' entries into the first
 * cluster of the newly allocated chain. Error handling is done in the driver
 */ 
int mkdir(struct BS_BPB * bs, const char * dirName, const char * extention, uint32_t targetDirectoryCluster) {
    struct DIR_ENTRY newDirEntry;
    
    //write directory entry to pwd
    uint32_t beginNewDirClusterChain = FAT_findFirstFreeCluster(bs);
    FAT_allocateClusterChain(bs, beginNewDirClusterChain);
    createEntry(&newDirEntry, dirName, extention, TRUE, beginNewDirClusterChain, 0, FALSE, FALSE);
    writeFileEntry(bs, &newDirEntry, targetDirectoryCluster, FALSE);
    //writing dot entries to newly allocated cluster chain
    writeFileEntry(bs, &newDirEntry, beginNewDirClusterChain, TRUE);
   
}
/*
 * success code 0: file found and removed
 * error code 1: file is open
 * error code 2: file is a directory
 * error code 3: file or directory not found
 * 
 * description: removes a file entry from the <searchDirectoryCluster> if
 * it exists. see error codes for other assumptions
 */ 
int rm(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) {
    struct DIR_ENTRY entry;
    struct FILEDESCRIPTOR fd;
    struct FILEDESCRIPTOR fileTableFd;
    uint32_t currentCluster = searchDirectoryCluster;
    int dirSizeInCluster = getFileSizeInClusters(bs, currentCluster);
    int fileTblIndex;
    int clusterCount;
    char fileName[12];
    int offset;
    int increment;
    
    uint32_t hitCluster;
    uint32_t hitFileFirstCluster;
    int entriesAfterHit = 0;
    int searchHitOffset = 0;
    bool hasSearchHit = FALSE;
    
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {
        if(strcmp(environment.pwd, ROOT) == 0) {
            offset = 1;
            increment = 2;
        } else {
            offset = 0;
            increment = 1;
        }
		
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
            if(strcmp(environment.pwd, ROOT) != 0 && offset == 2) {
                increment = 2;
                offset -= 1;	
                continue;
            }
			if(DEBUG == TRUE) printf("\rm: offset->%d\n", offset);
            readEntry(bs, &entry, currentCluster, offset);

            makeFileDecriptor(&entry, &fd);
            

            if(strcmp(searchName, fd.fullFilename) == 0) {
                //is the file a directory?
                if(entry.attributes == ATTR_DIRECTORY)
                    return 2;
                //can we find this file in the open file table?
                if( TBL_getFileDescriptor(&fileTblIndex, fd.fullFilename, FALSE) == TRUE ) {
                    if(environment.openFilesTbl[fileTblIndex].isOpen == TRUE)
                        return 1;
                }
                
                searchHitOffset = offset;
                hitCluster = currentCluster;
                hasSearchHit = TRUE;
                hitFileFirstCluster = fd.firstCluster;
                continue;
            }
            //use this to correctly set the empty entry symbol below
            if(hasSearchHit == TRUE) {
                if( isEntryEmpty(&fd) == FALSE ) {
                    entriesAfterHit++;
                }
            }
            
        }

       currentCluster = FAT_getFatEntry(bs, currentCluster); 
      
    }
    struct DIR_ENTRY entryToWrite;
    if(hasSearchHit == TRUE) {
        if(entriesAfterHit == 0) {
            //writing 0x00 at filename[0] of this entry
            createEntry(&entryToWrite, "", "", FALSE, 0, 0, FALSE, TRUE);
            writeEntry(bs, &entryToWrite, hitCluster, searchHitOffset);
            //this is to protect against writing to cluster 0 if the file
            //we are trying to remove has only been touched
            if(hitFileFirstCluster != 0) 
                FAT_freeClusterChain(bs, hitFileFirstCluster);
            return 0;
        } else {
            //writing 0x05 at filename[0] of this entry
            createEntry(&entryToWrite, "", "", FALSE, 0, 0, TRUE, FALSE);
            writeEntry(bs, &entryToWrite, hitCluster, searchHitOffset);
            if(hitFileFirstCluster != 0)
                FAT_freeClusterChain(bs, hitFileFirstCluster);
            return 0;
        }
    } else {
        return 3; 
        }
}


/*
 * success code 0: file or directory found and removed
 * error code 1: directory is not empty
 * error code 2: file is not a directory
 * error code 3: file or directory not found
 * 
 * description: removes a directory entry from the <searchDirectoryCluster> if
 * it exists. see error codes for other assumptions
 * 
 */ 
int rmDir(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) {
    struct DIR_ENTRY entry;
    struct FILEDESCRIPTOR fd;
    uint32_t currentCluster = searchDirectoryCluster;
    int dirSizeInCluster = getFileSizeInClusters(bs, currentCluster);
    if(DEBUG == TRUE) printf("dir Size: %d\n", dirSizeInCluster);
    int clusterCount;
    char fileName[12];
    int offset;
    int increment;
    uint32_t hitCluster; //the cluster where the entry is
    uint32_t hitFileFirstCluster;//the cluster where the entry's data starts
    int entriesAfterHit = 0;
    int searchHitOffset = 0;
    bool hasSearchHit = FALSE;
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {
        if(strcmp(searchName, ROOT) == 0) {
            offset = 1;
            increment = 2;
        } else {
            offset = 0;
            increment = 1;
        }
		
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
            if(strcmp(searchName, ROOT) != 0 && offset == 2) {
                increment = 2;
                offset -= 1;	
                continue;
            }

            readEntry(bs, &entry, currentCluster, offset);
            if(DEBUG == TRUE) printf("\ncluster num: %d\n", searchDirectoryCluster);
            makeFileDecriptor(&entry, &fd);
            
            if(strcmp(searchName, fd.fullFilename) == 0) {

                if( entry.attributes != ATTR_DIRECTORY )
                    return 2; //maybe we should return here to print error
                
                searchHitOffset = offset;
                hitCluster = currentCluster;
                hasSearchHit = TRUE;
                hitFileFirstCluster = fd.firstCluster;
                continue;
            }
            
            if(hasSearchHit == TRUE) {
                if( isEntryEmpty(&fd) == FALSE ) {
                    entriesAfterHit++;
                }
            }
            
        }

       currentCluster = FAT_getFatEntry(bs, currentCluster); 
      
    }
    //now traverse the data of the directory to see if it's empty
    if(hasSearchHit == TRUE) {
        offset = 0;
        increment = 1;
        for(; offset < ENTRIES_PER_SECTOR; offset += increment) {
             if(strcmp(searchName, ROOT) != 0 && offset == 2) {
                    increment = 2;
                    offset -= 1;	
                    continue;
                }
            readEntry(bs, &entry, hitFileFirstCluster, offset);
            makeFileDecriptor(&entry, &fd);
            
            if(isEntryEmpty(&fd) == FALSE) {
                if( (strcmp(".", fd.filename) != 0 ) && ( strcmp("..", fd.filename) != 0 ) ) {
                    return 1;
                }
            } 
        }
    }
    //write empty entries into pwd_cluster ,aliased as hitCluster here,
    //and free the cluster chain
   
    if(hasSearchHit == TRUE) {
        struct DIR_ENTRY entryToWrite;
        if(entriesAfterHit == 0) {
            createEntry(&entryToWrite, "", "", FALSE, 0, 0, TRUE, FALSE);
            writeEntry(bs, &entryToWrite, hitCluster, searchHitOffset);
            FAT_freeClusterChain(bs, hitFileFirstCluster);
            return 0;
        } else {
            createEntry(&entryToWrite, "", "", FALSE, 0, 0, FALSE, TRUE);
            writeEntry(bs, &entryToWrite, hitCluster, searchHitOffset);
            FAT_freeClusterChain(bs, hitFileFirstCluster);
            return 0;
        }
    }
    
    //if we reached here the file wasn't found    
    return 3; 
    
}
/* success code: 0 if fclose succeeded
 * error code 1: no filename given
 * error code 2: filename not found in pwd
 * error code 3: filename was never opened
 * error code 4: filename is currently closed 
 * 
 * description: attempts to change the status of <filename> in the open file table
 * from open to closed. see error codes above for assumptions
 * 
 */ 
int fClose(struct BS_BPB * bs, int argC, const char * filename) {
    int fIndex;
    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
     
        if(argC != 2) {
            return 1;
        }

         if(searchForFile(bs, filename, FALSE, searchFile) == FALSE) {
            return 2;
        }  

        if(TBL_getFileDescriptor(&fIndex, filename, FALSE) == FALSE ) {
            return 3;
        }

        if(TBL_getFileDescriptor(&fIndex,filename, FALSE) == TRUE && environment.openFilesTbl[fIndex].isOpen == FALSE) {
            return 4;
        }

        environment.openFilesTbl[fIndex].isOpen = FALSE;
        return 0;
    }

/* success code: 0 if fclose succeeded
 * error code 1: no filename given
 * error code 2: filename is a directory
 * error code 3: filename was not found
 * error code 4: filename is currently open 
 * error code 5: invalid option given
 * error code 6: file table was not updated
 * 
 * description: attempts to add <filename> to the open file table. if <filename> isn't
 * currently open and a valid file mode <option> is given the file is added to the file table.
 * <modeName> is passed in for the purose of displaying the selected mode to the user outside
 * of this function. See error codes above for other assumptions.
 * 
 * Assumptions: modeName must be at least 21 characters
 */ 
int fOpen(struct BS_BPB * bs, int argC, const char * filename, const char * option, char * modeName) {

    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
    struct FILEDESCRIPTOR * fileTblEntry;
    int fileMode;
    int fileTblIndex;
    
    if(argC != 3) {
        return 1;
    }
      
    if(searchForFile(bs, filename, TRUE, searchFile) == TRUE) {
        return 2;
    } 
    
    if(searchForFile(bs, filename, FALSE, searchFile) == FALSE) {
        return 3;
    }  
    
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == TRUE && environment.openFilesTbl[fileTblIndex].isOpen == TRUE) {
        return 4;
    } 

    if(strcmp(option, "w") == 0) {
        fileMode = MODE_WRITE;
        strcpy(modeName, "Writing");
    } else if(strcmp(option, "r") == 0) {
        fileMode = MODE_READ;
        strcpy(modeName, "Reading");
    } else if(strcmp(option, "x") == 0) {
        fileMode = MODE_BOTH;
        strcpy(modeName, "Reading and Writing");
    } else {
        return 5;
    }
    
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == FALSE ) {
        TBL_addFileToTbl(TBL_createFile(searchFile->filename, searchFile->extention, environment.pwd, searchFile->firstCluster, fileMode, searchFile->size, FALSE, TRUE), FALSE);
    } else {
        //update  entry if file is reopened
        environment.openFilesTbl[fileTblIndex].isOpen = TRUE;
        uint32_t offset;
        struct DIR_ENTRY entry;
        if( ( offset = getEntryOffset(bs, filename) ) == -1)
            return 6; 
            
        readEntry(bs, &entry, environment.pwd_cluster, offset);
        environment.openFilesTbl[fileTblIndex].size = entry.fileSize;
        environment.openFilesTbl[fileTblIndex].mode = fileMode;
       
    }
 
    return 0;
}

/* description: takes a cluster number of the first cluster of a file and its size. 
 * this function will write <dataSize> bytes starting at position <pos>. 
 */ 
int FWRITE_writeData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, const char * data, uint32_t dataSize){

            //determine the cluster offset of the cluster
            uint32_t currentCluster = firstCluster;
            // where pos is to start writing data
            uint32_t writeClusterOffset = (pos / bs->BPB_BytsPerSec);
            uint32_t posRelativeToCluster = pos %  bs->BPB_BytsPerSec;
            uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
            uint32_t remainingClustersToWalk = fileSizeInClusters - writeClusterOffset;
            if(DEBUG == TRUE)  printf("pos: %d, writeClusterOffset: %d, posRelativeToCluster: %d\n",pos, writeClusterOffset, posRelativeToCluster);
            int x;
            int dataIndex;
            //seek to the proper cluster offset from first cluster
            for(x = 0; x < writeClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            if(DEBUG == TRUE)  printf("after seek currentCluster is: %d\n", currentCluster);
            //if here, we just start writing at the last cluster right after
            //the last writen byte
            dataIndex = 0;
			uint32_t dataWriteLength = dataSize;
			uint32_t dataRemaining = dataSize;
			uint32_t fileWriteOffset;
			uint32_t startWritePos;
            FILE* f = fopen(environment.imageName, "r+");
            checkForFileError(f);
            for(x = 0; x < remainingClustersToWalk; x++) {
                
                startWritePos = byteOffsetOfCluster(bs, currentCluster);
                
                if(x == 0)
                    startWritePos += posRelativeToCluster;
                        
                if(dataRemaining > bs->BPB_BytsPerSec)
                    dataWriteLength = bs->BPB_BytsPerSec;
                else
                    dataWriteLength = dataRemaining;
                if(DEBUG == TRUE)  printf("right before write: dataWriteLength:%d\n", dataWriteLength);
                //write data 1 character at a time
                for(fileWriteOffset = 0; fileWriteOffset < dataWriteLength; fileWriteOffset++) {
                    fseek(f, startWritePos + fileWriteOffset, 0);
                    if(DEBUG == TRUE) printf("writing: %c to (startWritePos->%d + fileWriteOffset->) = %d, dataWriteLength: %d, dataRemaining:%d\n",data[dataIndex], startWritePos, fileWriteOffset,(startWritePos + fileWriteOffset),  dataWriteLength, dataRemaining );
                    uint8_t dataChar[1] = {data[dataIndex++]};
                    fwrite(dataChar, 1, 1, f);
                    
                }
                
                dataRemaining -= dataWriteLength;
                if(dataRemaining == 0){
                    if(DEBUG == TRUE)  puts("dataRemaining is 0\n");
                    break;                      
                }    
                
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(DEBUG == TRUE)  printf("Moving to cluster: %d", currentCluster);
            }
            fclose(f);
			return 0;
}
/* description: takes a cluster number of a file and its size. this function will read <dataSize>
 * bytes starting at position <pos>. It will print <dataSize> bytes to the screen until the end of
 * the file is reached, as determined by <fileSize>
 */ 
int FREAD_readData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, uint32_t dataSize, uint32_t fileSize){

            //determine the cluster offset of the cluster
            uint32_t currentCluster = firstCluster;
            // where pos is to start writing data
            uint32_t readClusterOffset = (pos / bs->BPB_BytsPerSec);
            uint32_t posRelativeToCluster = pos %  bs->BPB_BytsPerSec;
            uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
            uint32_t remainingClustersToWalk = fileSizeInClusters - readClusterOffset;
            if(DEBUG == TRUE)  printf("fread> pos: %d, readClusterOffset: %d, posRelativeToCluster: %d\n",pos, readClusterOffset, posRelativeToCluster);
            int x;
            //seek to the proper cluster offset from first cluster
            for(x = 0; x < readClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            if(DEBUG == TRUE) ("fread> after seek currentCluster is: %d\n", currentCluster);
            //if here, we just start writing at the last cluster right after
            //the last writen byte
			uint32_t dataReadLength = dataSize;
			uint32_t dataRemaining = dataSize;
			uint32_t fileReadOffset;
			uint32_t startReadPos;
            FILE* f = fopen(environment.imageName, "r");
            checkForFileError(f);
            for(x = 0; x < remainingClustersToWalk; x++) {
                
                startReadPos = byteOffsetOfCluster(bs, currentCluster);
                
                if(x == 0)
                    startReadPos += posRelativeToCluster;
                        
                if(dataRemaining > bs->BPB_BytsPerSec)
                    dataReadLength = bs->BPB_BytsPerSec;
                else
                    dataReadLength = dataRemaining;
                if(DEBUG == TRUE)  printf("fread> right before write: dataReadLength:%d\n", dataReadLength);
                //write data 1 character at a time
                for(fileReadOffset = 0; fileReadOffset < dataReadLength && (pos + fileReadOffset) < fileSize; fileReadOffset++) {
                    fseek(f, startReadPos + fileReadOffset, 0);
                    if(DEBUG == TRUE)  printf("fread> (startReadPos->%d + fileReadOffset->%d) = %d, dataReadLength: %d, dataRemaining:%d\n", startReadPos, fileReadOffset,(startReadPos + fileReadOffset),  dataReadLength, dataRemaining );
                    uint8_t dataChar[1];
                    fread(dataChar, 1, 1, f);
                    printf("%c", dataChar[0]);
                }
                
                dataRemaining -= dataReadLength;
                if(dataRemaining == 0){
                    if(DEBUG == TRUE)  puts("fread> dataRemaining is 0\n");
                    break;                      
                }    
                
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(DEBUG == TRUE)  printf("fread> Moving to cluster: %d\n", currentCluster);
            }
            fclose(f);
			return 0;
}



/* success code: 0 if fwrite succeeded
 * error code 1: filename is a directory
 * error code 2: filename was never opened
 * error code 3: filename is not currently open
 * error code 4: no data to be written
 * error code 5: not open for writing
 * error code 6: filename does not exist in pwd
 * error code 7: empty file entry could allocated space
 * error code 8: file entry could be updated w/ new file size
 */ 
 /* description: Takes a filename, checks if file has been opened for writing. If so
  * it uses FWRITE_writeData() to write <data> to <filename>. If <position> is larger 
  * than the current filesize this grows it to the size required and fills the gap created with 0x20 (SPACE).
  * After writing if the filesize has changed this updates the directory entry of <filename> the new size
  */ 
int fWrite(struct BS_BPB * bs, const char * filename, const char * position, const char * data) {
    bool debug = TRUE;
    int fileTblIndex;
    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
    struct DIR_ENTRY entry;
    uint32_t offset; //for updating directory entry
    
    uint32_t pos = atoi(position);
    uint32_t firstCluster;
    uint32_t currentCluster;
    uint32_t dataSize = strlen(data);
    uint32_t fileSize;
    uint32_t newSize;
    
    uint32_t totalSectors;
    uint32_t additionalsectors;
    uint32_t paddingClusterOffset;

    uint32_t remainingClustersToWalk;
    
    uint32_t paddingLength = 0;
    uint32_t paddingRemaining;    
    
    uint32_t usedBytesInLastSector = 0;
    uint32_t openBytesInLastSector = 0;
    uint32_t startWritePos;
    
    uint32_t paddingWriteLength;
        
    uint32_t fileWriteOffset;

    char * dataChar;
    int x;
    //error checking
    if(searchForFile(bs, filename, TRUE, searchFile) == TRUE) 
        return 1; 
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == FALSE)
        return 2;
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == TRUE && environment.openFilesTbl[fileTblIndex].isOpen == FALSE)
        return 3;
    if(dataSize < 1)
        return 4;
    if(environment.openFilesTbl[fileTblIndex].mode != MODE_WRITE && environment.openFilesTbl[fileTblIndex].mode != MODE_BOTH) 
        return 5;
    if(searchForFile(bs, filename, FALSE, searchFile) == FALSE) 
        return 6;
    
    FILE* f = fopen(environment.imageName, "r+");
    checkForFileError(f);
    //if file was touched, no cluster chain was allocated, do it here
    if(searchFile->firstCluster == 0) {
        if( ( offset = getEntryOffset(bs, filename) ) == -1)
            return 7;
        readEntry(bs,  &entry, environment.pwd_cluster, offset);
        
        firstCluster = FAT_findFirstFreeCluster(bs);
        FAT_allocateClusterChain(bs, firstCluster);
        
        deconstructClusterAddress(&entry, firstCluster);
        if(DEBUG == TRUE)  printf("Retreiving Entry From Offset %d, First cluster allocated %d", offset, firstCluster);
        environment.openFilesTbl[fileTblIndex].firstCluster = firstCluster;//set firstCluster
        environment.openFilesTbl[fileTblIndex].size = 0;                    // set size, althoug it's probaly set
        writeEntry(bs,  &entry, environment.pwd_cluster, offset);
    } else{
        fileSize = environment.openFilesTbl[fileTblIndex].size;
        firstCluster = currentCluster = environment.openFilesTbl[fileTblIndex].firstCluster;
    }
    //get max chain size for walking the chain
    uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
    
    if(DEBUG == TRUE)  printf("fwrite> data:%s, data length:%d, pos:%d, fileSize:%d, firstCluster:%d \n", data, dataSize, pos, fileSize, firstCluster);
    if(DEBUG == TRUE)  printf("fwrite> (pos + dataSize):%d,  \n", (pos + dataSize));
    
    if((pos + dataSize) > fileSize) {
        //cluster size must grow
        if(DEBUG == TRUE)  puts("case 1\n");
        newSize = pos + dataSize;
        
        totalSectors = newSize / bs->BPB_BytsPerSec;
        if(newSize > 0 && (newSize % bs->BPB_BytsPerSec != 0))
            totalSectors++;
        
        //determine newsize of chain
        additionalsectors = totalSectors - fileSizeInClusters;
        
        //grow the chain to new necessary size
        for(x = 0; x < additionalsectors; x++) {
            if(DEBUG == TRUE)  printf("\nExtending chain: %d\n", x);
            FAT_extendClusterChain(bs, firstCluster);
        }
        
        //reacquire new size
        fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
        
        //create padding with 0x20
        
        if(pos > fileSize)
            paddingLength = pos - fileSize;
        else
            paddingLength = 0;
            
        paddingRemaining = paddingLength;
        //determine used/open bytes in the last sector
        if(fileSize > 0) {
            if((fileSize % bs->BPB_BytsPerSec) == 0)    
                usedBytesInLastSector = bs->BPB_BytsPerSec;
            else
                usedBytesInLastSector = fileSize % bs->BPB_BytsPerSec;
        }
        
        openBytesInLastSector = bs->BPB_BytsPerSec - usedBytesInLastSector;
       if(DEBUG == TRUE)  printf("totalSectors: %d,additionalsectors: %d,fileSizeInClusters: %d,paddingLength: %d, usedBytesInLastSector: %d,openBytesInLastSector: %d\n",totalSectors,additionalsectors,fileSizeInClusters,paddingLength, usedBytesInLastSector,openBytesInLastSector);
        if(paddingLength > 0) {
           if(DEBUG == TRUE)  puts("padding \n");
            //determine the cluster offset of the last cluster
            //to start padding 
            paddingClusterOffset = (fileSize / bs->BPB_BytsPerSec);
            
            //set remaining number of clusters to be walked til EOC
            remainingClustersToWalk = fileSizeInClusters - paddingClusterOffset;
            
            //seek to the proper cluster offset
            for(x = 0; x < paddingClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            
            uint8_t padding[1] = {0x20};
            if(DEBUG == TRUE)  printf("paddingClusterOffset: %d, remainingClustersToWalk: %d,currentCluster: %d\n",paddingClusterOffset, remainingClustersToWalk,currentCluster);
            //determine where start writing padding
            for(x = 0; x < remainingClustersToWalk; x++) {
                if(x == 0) {
                    startWritePos = byteOffsetOfCluster(bs, currentCluster) + usedBytesInLastSector;
                    if((usedBytesInLastSector + paddingRemaining) > bs->BPB_BytsPerSec)
                        paddingWriteLength = openBytesInLastSector;
                    else
                        paddingWriteLength = paddingRemaining;
                    
                }
                else {
                    startWritePos = byteOffsetOfCluster(bs, currentCluster);
                    if(paddingRemaining > bs->BPB_BytsPerSec)
                        paddingWriteLength = bs->BPB_BytsPerSec;
                    else
                        paddingWriteLength = paddingRemaining;
                    
                    
                }

                //write padding
                for(fileWriteOffset = 0; fileWriteOffset < paddingWriteLength ; fileWriteOffset++) {
                    fseek(f, startWritePos + fileWriteOffset, 0);
                    fwrite(padding, 1, 1, f);
                }
                
                paddingRemaining -= paddingWriteLength;
                if(paddingRemaining == 0) {
                    if(DEBUG == TRUE)  puts("paddingRemaining is 0\n");
                    break;                      
                }
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(DEBUG == TRUE)  printf("Moving tp cluster: %d", currentCluster);
            }  
        } //end padding if
        
        if(DEBUG == TRUE)  puts("Starting Data Write From Case 1\n");
        FWRITE_writeData(bs, firstCluster, pos, data, dataSize);

    } else {
        //cluster size will not grow
        newSize = fileSize;
        if(DEBUG == TRUE) puts("Starting Data Write From Case 2\n");
        FWRITE_writeData(bs, firstCluster, pos, data, dataSize);
    }
    
    
        
    //update filesize
    
    if(fileSize != newSize) {
        if( ( offset = getEntryOffset(bs, filename) ) == -1)
            return 8;
        readEntry(bs,  &entry, environment.pwd_cluster, offset);
        entry.fileSize = newSize;
        if(DEBUG == TRUE)  printf("Retreiving Entry From Offset %d, filesize is now ", offset, entry.fileSize);
        environment.openFilesTbl[fileTblIndex].size = newSize;
        writeEntry(bs,  &entry, environment.pwd_cluster, offset);
    }
    fclose(f);
    return 0;
    
}

/* success code: 0 if fwrite succeeded
 * error code 1: filename is a directory
 * error code 2: filename was never opened
 * error code 3: filename is not currently open
 * error code 4: position is greater than filesize
 * error code 5: not open for reading
 * error code 6: filename does not exist in pwd
 */ 
 /* description: Takes a filename, checks if file has been opened for reading. If so
  * it uses FREAD_readData() to print <numberOfBytes> to the screen, starting at <position>
  * 'th byte. <actualBytesRead> is to be passed back to the caller
  */ 
int fRead(struct BS_BPB * bs, const char * filename, const char * position, const char * numberOfBytes, uint32_t * actualBytesRead) {
    bool debug = FALSE;
    int fileTblIndex;
    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
    struct DIR_ENTRY entry;
    uint32_t offset; //for updating directory entry
    
    uint32_t pos = atoi(position);
    uint32_t dataSize = atoi(numberOfBytes);
    
    uint32_t firstCluster;
    uint32_t currentCluster;
    uint32_t fileSize;
    uint32_t newSize;

    char * dataChar;
    int x;

    //error checking
    if(searchForFile(bs, filename, TRUE, searchFile) == TRUE) 
        return 1; 
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == FALSE)
        return 2;
    if(TBL_getFileDescriptor(&fileTblIndex, filename, FALSE) == TRUE && environment.openFilesTbl[fileTblIndex].isOpen == FALSE)
        return 3;
    if(pos > environment.openFilesTbl[fileTblIndex].size)
        return 4;
    if(environment.openFilesTbl[fileTblIndex].mode != MODE_READ && environment.openFilesTbl[fileTblIndex].mode != MODE_BOTH) 
        return 5;
    if(searchForFile(bs, filename, FALSE, searchFile) == FALSE) 
        return 6;
    
    FILE* f = fopen(environment.imageName, "r");
    checkForFileError(f);
    fileSize = environment.openFilesTbl[fileTblIndex].size;
    *actualBytesRead = fileSize; //to be passed back to the caller
    firstCluster = currentCluster = environment.openFilesTbl[fileTblIndex].firstCluster;
    FREAD_readData(bs, firstCluster, pos, dataSize, fileSize);

    fclose(f);
    return 0;
}

//------ARGUMENT PARSING UTILITIES --------------
//puts tokens into argV[x], returns number of tokens in argC
int tokenize(char * argString, char ** argV, const char * delimiter)
{
	int index = 0;
    char * tokens =  strtok(argString, delimiter);
	while(tokens != NULL) {
		argV[index] = tokens;
		tokens = strtok(NULL, delimiter);
		index++;
	}
	
	return index;
}
/* description: if a path character exists mark this as a path
 * <delimiter> is a single character string
 */ 
int isPathName(const char * pathName, const char * delimiter) {
    int pathLength = strlen(pathName);
    int x; 
    for(x = 0; x< pathLength; x++) {
        if(pathName[x] == delimiter[0])
            return TRUE;
    }
    return FALSE;
}

/*
* error code 1: too many invalid characters
* error code 2: extention given but no filename
* error code 3: filename longer than 8 characters
* error code 4: extension longer than 3 characters
* error code 5: cannot touch or mkdir special directories
* success code 0: filename is valid
*/

int filterFilename(const char * argument, bool isTouchOrMkdir, bool isCdOrLs){
	char filename[200];
	int invalid = 0;
	int periodCnt = 0;
	int tokenCount = 0;
	char * tokens[10];
    bool periodFlag = FALSE;
    int x;

	strcpy(filename, argument);

	// '.' and '..' are not allowed with touch or mkdir
	if(isTouchOrMkdir == TRUE && (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0))
		return 5;

	//cd and ls will allow special directories
	if(isCdOrLs == TRUE && (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0))
		return 0;

	for(x = 0; x < strlen(filename); x++) {
		if(
			((filename[x] >= ALPHA_NUMBERS_LOWER_BOUND && filename[x] <= ALPHA_NUMBERS_UPPER_BOUND) || 
			(filename[x] >= ALPHA_UPPERCASE_LOWER_BOUND && filename[x] <= ALPHA_UPPERCASE_UPPER_BOUND)) == FALSE) 
			{      
					invalid++;   
					if(filename[x] == PERIOD) {
						periodCnt++;
                        periodFlag = TRUE;
                    }
			} 
	}

	if(invalid > 1 ) {//more than 1 invalid character
		return 1;
	} else 
    if(invalid == 1 && periodCnt == 0) {//invalid that's not a period
        return 1;
    } else
    if(invalid == 1 && periodCnt == 1) { // a single period

			tokenCount = tokenize(filename, tokens, ".");

			if(tokenCount < 2) { //is filename or ext missing?
				return 2;
			}

			if(strlen(tokens[0]) > 8) { //is filename > 8?
				return 3;
			}

			if(strlen(tokens[1]) > 3) { //is ext < 3
				return 4;
			}
            return 0; //we've passed all check for filename with '.'
    } else
     if(invalid == 0)  { //there's no period
		//is filename > 8?
		if(strlen(filename) > 8) {
			return 3;
		} else
            return 0; //we've passed all check for filename with no extenion
	}
	//re-tokenize once we exit here
}
//this does just what it sounds like
int printFilterError(const char * commandName, const char * filename, bool isDir, int exitCode) {
	char fileType[11];
    
	if(isDir == TRUE)
		strcpy(fileType, "directory");
	else
		strcpy(fileType, "file");
		
	switch(exitCode) {
		case 1:
			printf("\nERROR: \"%s\" not a valid %s name: only uppercase alphameric characters allowed\n", filename, fileType);
			break;
		case 2:
			printf("\nERROR: %s failed on filename %s, name or extention not given", commandName, fileType);
			break;
		case 3:
			printf("\nERROR: %s failed, %s name \"%s\" longer than 8 characters", commandName, fileType, filename );
			break;
		case 4:
			printf("\nERROR: %s failed, extention \"%s\" longer than 3 characters", commandName, filename);
			break;
		case 5:
			printf("\nERROR: %s failed, cannot \"%s\" using \"%s\" as a name", commandName, commandName, filename);
			break;
	
	}
}
/* description: the name is a misnomer, it handles absolute and relative paths.
 * this uses ls to traverse the <path> until reaches the end. If it reaches the end 
 * then all path components were valid directories and it is resolved. We then
 * pass first cluster of the target directory of the path back using 
 * <targetCluster> If not we pass the directory name that caused the 
 * resolution to fail back to the caller with <failFilename>. if <isCd>
 * is set we try to resolve and if we care able to do that then we cd
 * into the target directory using a iterative loop. For functions like
 * touch we need to stop the resolution one token before the end of the path
 * and pass back that cluster number through <targetcluster> so shorten
 * the iterations by 1 if <searchFile> is set. <isMkdir> is DEPRECATED
 * and slated for removal.
 */ 
bool handleAbsolutePaths(struct BS_BPB * bs, uint32_t * targetCluster, char * path, char * successFilename, char * failFilename, bool isCd, bool searchForFile, bool isMkdir) {
    int x;
    char * paths[400];
    int pathsArgC;
    bool isValidPath = TRUE;
    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
    uint32_t pwdCluster = *targetCluster;
    uint32_t previousCluster;
    
    if(strcmp(path, ".") == 0) {
        *targetCluster = environment.pwd_cluster;
        strcpy(successFilename, environment.pwd);
        return TRUE;
    }
    
    //if this is absolute path
    if(path[0] == PATH_DELIMITER[0])
        pwdCluster = *targetCluster = bs->BPB_RootClus;
    pathsArgC = tokenize(path, paths, PATH_DELIMITER);
    
    //we resolve for directories
    //because the last token will be a file we stop iterating right before it
    if(searchForFile == TRUE)
        pathsArgC -= 1;
        
    for(x = 0; x < pathsArgC; x++) {    
        if( ls(bs, *targetCluster, TRUE, paths[x], FALSE, searchFile, TRUE, TRUE) == TRUE ) {
            previousCluster = *targetCluster;
            *targetCluster = searchFile->firstCluster;
        } else {
            strcpy(failFilename, paths[x]); // pass back to caller
            if(DEBUG == TRUE) printf("failed on %s\n", failFilename);
            if(isCd == TRUE)
                isValidPath = FALSE;
            else
                return FALSE;
            break;
        }
    }//end for
    if(isCd == TRUE) {
        
        if(isValidPath == FALSE)
            return FALSE;
        else {
            *targetCluster = pwdCluster;
            //if validPath is TRUE we know we can safely walk to the intended dir
            for(x = 0; x < pathsArgC; x++) {
                ls(bs, *targetCluster, TRUE, paths[x], FALSE, searchFile, FALSE, FALSE);
                *targetCluster = searchFile->firstCluster;
                strcpy(environment.pwd, paths[x]);
            }
            //strcpy(environment.pwd, paths[pathsArgC - 1]);
            return TRUE;
        }
        
    } else {
        //because we subtracted by 1 above we can get the last elements 
        //by indexing the size and we won't bound out
        if(searchForFile == TRUE)
            strcpy(successFilename, paths[pathsArgC]);
        else
            strcpy(successFilename, paths[pathsArgC - 1]);

        if(isMkdir == TRUE)
            *targetCluster = previousCluster;
            
        return TRUE;
    
    }
}
//------------ INITIALIZATION ------------------------------
/* loads up boot sector and initalizes file and directory tables

*/

void allocateFileTable() {
    //initalize and allocate open directory table
	environment.dirStatsTbl =  malloc(TBL_DIR_STATS * sizeof(struct FILEDESCRIPTOR));
	int i;
	for (i = 0; i < TBL_DIR_STATS; i++)
		environment.dirStatsTbl[i] = *( (struct FILEDESCRIPTOR *) malloc( sizeof(struct FILEDESCRIPTOR) ));

	//initalize and allocate open files table
	environment.openFilesTbl =  malloc(TBL_OPEN_FILES * sizeof(struct FILEDESCRIPTOR));
	for (i = 0; i < TBL_OPEN_FILES; i++)
		environment.openFilesTbl[i] = *( (struct FILEDESCRIPTOR *) malloc( sizeof(struct FILEDESCRIPTOR) ));
}


int initEnvironment(const char * imageName, struct BS_BPB * boot_sector, bool isDebug) {
    
    DEBUG = isDebug;
	FILE* f = fopen(imageName, "r");
    if(f == NULL) {
        return 1;
    }
    strcpy(environment.imageName, imageName);
	fread(boot_sector, sizeof(struct BS_BPB), 1,f);
    fclose(f);
    strcpy(environment.pwd, ROOT);
    strcpy(environment.last_pwd, ROOT);
	
    environment.pwd_cluster = boot_sector->BPB_RootClus;
    environment.tbl_dirStatsIndex = 0;
    environment.tbl_dirStatsSize = 0;
    environment.tbl_openFilesIndex = 0;
    environment.tbl_openFilesSize = 0;
	
    FAT_setIoWriteCluster(boot_sector, environment.pwd_cluster);
    allocateFileTable();

    return 0;
}

void showPromptAndClearBuffer(char * buffer) {
    printf("\n%s:%s> ",environment.imageName, environment.pwd);
    bzero(buffer, 100);
}

int main(int argc, char *argv[], char *envp[])
{
	struct BS_BPB boot_sector;
    struct DIR_ENTRY dir;
    struct FILEDESCRIPTOR searchFileInflator;
    struct FILEDESCRIPTOR * searchFile = &searchFileInflator;
    
    char inputChar;
    char inputBuffer[200];
    char inputBuffer2[200];
    char * my_argv[400];
    
    //paths
    char successFilename[200];
    char failFilename[200];
    
    char pwd[100];
    bzero(inputBuffer, 100);
    int argC = 0;
    int length = 0;
    char * tokens[10];
	int exitCode;
    bool isDebug = FALSE;
    
    if(argc == 3) {
        if(strcmp(argv[2], "-d") == 0) {
            isDebug = TRUE;
            puts("\fedit is running in debug mode\n");
        } else {
            puts("\nUsage: ./fedit <image name> [-d]\n");
            exit(EXIT_FAILURE);
        }   
    }

	if(initEnvironment(argv[1], &boot_sector, isDebug) == 1) {
        printf("\nFATAL ERROR: image not found\n", environment.imageName);
        return 1;
    }
	
	
    printf("%s:%s> ",argv[1], environment.pwd);
    
    while(inputChar != EOF) 
    {
        inputChar = getchar();

        switch(inputChar) 
        {
			case '\n': 
				
				if(inputBuffer[0] != '\0') //SOMETHING WAS TYPED IN
			    {
					strncat(inputBuffer, "\0", 1); 
                    length = strlen(inputBuffer);
                    if(length > 200) {
                        puts("\nERROR: Maximum buffer length of 200 exceeded\n");
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    }
                    strcpy(inputBuffer2, inputBuffer);

					argC = tokenize(inputBuffer, my_argv, " ");

                    if(strcmp(my_argv[0], "exit") == 0) //EXIT COMMAND
                    {
                        inputChar = EOF;
                        return EXIT_SUCCESS;
                        break;
                    } else
                    
                    if(strcmp(my_argv[0], "ls") == 0) //LS COMMAND
                    {
                        if(argC > 2) {
                            puts("\nUsage: ls [<path>]\n");
                    //if a path was used
                    } else if(argC == 2) { 
                        uint32_t currentCluster = environment.pwd_cluster;
                        if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE && strcmp(my_argv[1], ROOT) != 0) {
                            //if abs/relative ath
                            if(handleAbsolutePaths(&boot_sector, &currentCluster, my_argv[1], successFilename, failFilename, FALSE, FALSE, FALSE) == TRUE ) {
                                if((exitCode = filterFilename(successFilename, FALSE, TRUE)) != SUCCESS) {
                                     printFilterError("ls", my_argv[1], TRUE, exitCode);
                                     printf("\n%s:%s> ",argv[1], environment.pwd);
                                     bzero(inputBuffer, 100);
                                     break;
                                }
                                ls(&boot_sector, currentCluster, FALSE, successFilename, FALSE, searchFile, FALSE, FALSE);
                            }    
                            else 
                                printf("ERROR: : %s is not a valid directory", failFilename);
                                
                        } else if(argC > 1 && strcmp(my_argv[1], ROOT) != 0){
                            //looking down a single level
                            if((exitCode = filterFilename(my_argv[1], FALSE, TRUE)) != SUCCESS) {
                                 printFilterError("ls", my_argv[1], TRUE, exitCode);
                                 printf("\n%s:%s> ",argv[1], environment.pwd);
                                 bzero(inputBuffer, 100);
                                 break;
							 }
                            
                            if(searchForFile(&boot_sector, my_argv[1], TRUE, searchFile) == TRUE) 
                                ls(&boot_sector, searchFile->firstCluster, FALSE, my_argv[1], FALSE, searchFile, FALSE, FALSE);
                            else
                                printf("ERROR: : %s is not a valid directory", my_argv[1]);
                        } else if(argC > 1 && strcmp(my_argv[1], ROOT) == 0){
                        //ls /

                        ls(&boot_sector, boot_sector.BPB_RootClus, FALSE, ROOT, FALSE, searchFile, FALSE, FALSE);
                        }   
                    }else {
                        //no path was used
                        ls(&boot_sector, environment.pwd_cluster, FALSE, environment.pwd, FALSE, searchFile, FALSE, FALSE);
                    }
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else
					
                    if(strcmp(my_argv[0], "info") == 0) //INFO COMMAND
                    {
                        if(argC > 1) {
							puts("\nUsage: info\n");
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                    
                        puts("Current Environment\n");
                        if(DEBUG == TRUE) printf("pwd: %s\npwd_cluster: %d\nio_writeCluster: %d\nlast_pwd: %s",
                        environment.pwd, environment.pwd_cluster, environment.io_writeCluster, environment.last_pwd); 
                        showDriveDetails(&boot_sector);
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else
					
					if(strcmp(my_argv[0], "show") == 0) //SHOW COMMAND
                    {
                        TBL_printFileTbl(TRUE);
						TBL_printFileTbl(FALSE);
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "size") == 0) //SIZE COMMAND 
                    {
						
                        if(argC != 2) {
							puts("\nUsage: size <filename>\n");
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        
                        if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("size", my_argv[1], FALSE, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
						if(printFileOrDirectorySize(&boot_sector, my_argv[1], searchFile) == FALSE)
                            printf("ERROR: %s not found\n", my_argv[1]);
                        //searchOrPrintFileSize(&boot_sector, my_argv[1], FALSE, FALSE, searchFile);
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "rm") == 0) //RM COMMAND 
                    //searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile)
                    {
                        if(argC != 2) {
							puts("\nUsage: rm <filename | path>\n");
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        
                        if(isPathName(my_argv[1], PATH_DELIMITER) == FALSE) {
                            if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
                                 printFilterError("rm", my_argv[1], FALSE, exitCode);
                                 printf("\n%s:%s> ",argv[1], environment.pwd);
                                 bzero(inputBuffer, 100);
                                 break;
                                 }
                         }
                        int returnCode;
                        uint32_t targetCluster = environment.pwd_cluster;
 
						if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE && handleAbsolutePaths(&boot_sector, &targetCluster, my_argv[1], successFilename, failFilename, FALSE, TRUE, FALSE) == TRUE ) {
                            returnCode = rm(&boot_sector, successFilename, targetCluster);
                            strcpy(my_argv[1], successFilename);
                        }
                        else
                            returnCode = rm(&boot_sector, my_argv[1], environment.pwd_cluster);
                        
                        switch(returnCode) {
                         case 0:
                            printf("rm Success: \"%s\" has been removed", my_argv[1]);
                            break;
                        case 1:
                            printf("ERROR: %s failed, \"%s\" is currently open", "rm", my_argv[1]);
                            break;
                        case 2:
                            printf("ERROR: %s failed, \"%s\" is a directory", "rm", my_argv[1]);
                            break;
                        case 3:
                            printf("ERROR: %s failed, \"%s\" not found", "rm", my_argv[1]);
                            break;
                        } 
                        
                        
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "rmdir") == 0) //RMDIR COMMAND 
                    //searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile)
                    {
                        if(argC != 2) {
							puts("\nUsage: rmdir <filename | path>\n");
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        if(isPathName(my_argv[1], PATH_DELIMITER) == FALSE) {
                            if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
                                 printFilterError("rmdir", my_argv[1], TRUE, exitCode);
                                 printf("\n%s:%s> ",argv[1], environment.pwd);
                                 bzero(inputBuffer, 100);
                                 break;
                                 }
                            }
                        //handleAbsolutePaths(struct BS_BPB * bs, uint32_t * targetCluster, char * path, char * successFilename, char * failFilename, bool isCd, bool searchForFile)
                        int returnCode;
                        uint32_t targetCluster = environment.pwd_cluster;
						if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE && handleAbsolutePaths(&boot_sector, &targetCluster, my_argv[1], successFilename, failFilename, FALSE, FALSE, TRUE) == TRUE ) {
                            returnCode = rmDir(&boot_sector, successFilename, targetCluster);
                            strcpy(my_argv[1], successFilename);
                        }
                        else
                            returnCode = rmDir(&boot_sector, my_argv[1], environment.pwd_cluster);
                            
							
                         switch(returnCode) {
                             case 0:
                                 printf("rm Success: \"%s\" has been removed\n", my_argv[1]);
                                break;
                            case 1:
                                printf("ERROR: %s failed, \"%s\" not empty\n", "rmdir", my_argv[1]);
                                break;
                            case 2:
                                printf("ERROR: %s failed, \"%s\" is not a directory\n", "rmdir", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: %s failed, \"%s\" not found\n", "rmdir", my_argv[1]);
                                break;
                         } 
                        
                        
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else
                    
                    if(strcmp(my_argv[0], "cd") == 0) //CD COMMAND
                    {
						if(argC != 2) {
							puts("\nUsage: cd <directory_name | path>\n");
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        //don't snag path names
						if(isPathName(my_argv[1], PATH_DELIMITER) == FALSE) {
                            if((exitCode = filterFilename(my_argv[1], FALSE, TRUE)) != SUCCESS) {
                                 printFilterError("cd", my_argv[1], TRUE, exitCode);
                                 printf("\n%s:%s> ",argv[1], environment.pwd);
                                 bzero(inputBuffer, 100);
                                 break;
                                 }
                         }
						
                        //printf("my_argv[1]%s\n", my_argv[1]);
						if(strcmp(my_argv[1], SELF) == 0) // going to '.'
							;
						else if(strcmp(my_argv[1], ROOT) == 0) {// going to '/'
                             strcpy(environment.last_pwd, ROOT);
                             strcpy(environment.pwd, ROOT);
                             environment.pwd_cluster = boot_sector.BPB_RootClus;
                             FAT_setIoWriteCluster(&boot_sector, environment.pwd_cluster);
                        } else if(strcmp(my_argv[1], PARENT) == 0) {
                            //going up the tree
                            if(cd(&boot_sector, PARENT, TRUE, searchFile) == TRUE) {
                            } else {
                                
                                puts("\n ERROR: directory not found: '..'\n");
                            }
                        } else {//going down the tree
                      
                            if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE) {
                                uint32_t currentCluster = environment.pwd_cluster;
                                if(handleAbsolutePaths(&boot_sector, &currentCluster, my_argv[1], successFilename, failFilename, TRUE, FALSE, FALSE) == FALSE) 
                                    printf("\nERROR: directory not found: %s\n", failFilename);
                            } else if(cd(&boot_sector, my_argv[1], FALSE, searchFile) == TRUE) {
                                strcpy(environment.last_pwd, environment.pwd);
                                strcpy(environment.pwd, my_argv[1]);
                            } else {
                                if(cd(&boot_sector, PARENT, FALSE, searchFile) == TRUE)
                                    printf("\n ERROR: \"%s\" is a file:\n", my_argv[1]);
                                else
                                    printf("\nERROR: directory not found: %s\n", my_argv[1]);
                            }
                        }
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else
                    
                    if(strcmp(my_argv[0], "touch") == 0 || strcmp(my_argv[0], "mkdir") == 0) //TOUCH & MKDIR COMMAND
                    {
                        int x;
                        int tokenCnt = 0;
						bool isMkdir = FALSE; //is the file we are creating a directory?
						char commandName[6];
                        char fileType[10];
                        char fullFilename[13];

                        if( strcmp(my_argv[0], "mkdir") == 0) {   
                            isMkdir = TRUE;
                            strcpy(commandName, "mkdir");
                            strcpy(fileType, "directory");
                        } else {
                            strcpy(commandName, "touch");
                            strcpy(fileType, "file");
                        }
                        
						//if only "touch" is typed with no args
						if(argC != 2) {
                            printf("\nUsage: %s <[path to] %sname>\n", commandName, fileType);
							printf("\n%s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						
						}

                         uint32_t targetCluster = environment.pwd_cluster;
                        //if there's a path try to resolve it
                         if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE) { 
                            if(handleAbsolutePaths(&boot_sector, &targetCluster, my_argv[1], successFilename, failFilename, FALSE, TRUE, FALSE) == TRUE ) {
                                if(DEBUG == TRUE) printf("targetCluster: %d\n", targetCluster);
                                strcpy(my_argv[1], successFilename);
                            } else {
                                printf("ERROR: Directory not found: \"%s\"\n", failFilename);
                                printf("\n%s:%s> ",argv[1], environment.pwd);
                                bzero(inputBuffer, 100);
                                break;
                            }
                        }

                        //check if filename is valid
                        if((exitCode = filterFilename(my_argv[1], TRUE, FALSE)) != SUCCESS) {
							 printFilterError(commandName, my_argv[1], isMkdir, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
                             
                        strcpy(fullFilename, my_argv[1]);
                        tokenCnt = tokenize(my_argv[1], tokens, ".");
                        
                        if(isMkdir == TRUE) 
                        {   //check if filename exists (whether dir or file)
                            if(searchForFile(&boot_sector, fullFilename, isMkdir, searchFile) == TRUE ||
                                searchForFile(&boot_sector, fullFilename, !isMkdir, searchFile) == TRUE) {
                                printf("ERROR: %s failed, \"%s\" already exists", commandName, fullFilename);
                            }
                            else {
                                if(tokenCnt > 1)
                                    mkdir(&boot_sector, tokens[0], tokens[1], targetCluster);
                                else
                                    mkdir(&boot_sector, tokens[0], "", targetCluster);
                                    
                                printf("%s \"%s\" created\n", fileType, my_argv[1]);
                            }
                        }
                        else 
                        {
                            if(searchForFile(&boot_sector, fullFilename, isMkdir, searchFile) == TRUE ||
                                searchForFile(&boot_sector, fullFilename, !isMkdir, searchFile) == TRUE) {
                                printf("ERROR: %s failed, \"%s\" already exists", commandName, fullFilename);
                            }
                            else {
                                if(tokenCnt > 1)
                                    touch(&boot_sector, tokens[0], tokens[1], targetCluster);
                                else
                                    touch(&boot_sector, tokens[0], "", targetCluster);
                                
                                printf("%s \"%s\" created\n", fileType, my_argv[1]);
                            }
                        }
                        
                    
                      
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fopen") == 0) //FOPEN COMMAND
                    {
                        char modeName[21];
						
                        if(argC < 2) {
                            puts("\nUsage: fopen <filename> <r | w | x>\n");
                            printf("\n%s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
                        
						if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fopen", my_argv[1], FALSE, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
						
                        switch(fOpen(&boot_sector, argC, my_argv[1], my_argv[2], modeName)) {
                            case 0:
                                 printf("File \"%s\" opened for %s", my_argv[1], modeName);                           
                                break;
                            case 1:
                                puts("\nUsage: fopen <filename> <r | w | x>\n");
                                break;
                            case 2:
                                printf("ERROR: fopen failed, \"%s\" is a directory\n", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: fopen failed, \"%s\" not found\n", my_argv[1]);
                                break;
                            case 4:
                                printf("ERROR: fopen failed, \"%s\" is already open\n", my_argv[1]);
                                break;
                            case 5:
                                printf("ERROR: fopen failed, \"%s\" is not a valid option\n", my_argv[2]);
                                puts("\nUsage: fopen <filename> <r | w | x>\n");
                                break;
                            case 6:
                                printf("ERROR: fopen succeeded, but the filetable could not be updated\n", my_argv[1]);
                                break;
                        }
                        
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                        
                        
                    } else 
                    
                    if(strcmp(my_argv[0], "fclose") == 0) //FCLOSE COMMAND
                    {
                        if(argC != 2) {
                            puts("\nUsage: fclose <filename>\n");
                            printf("\n%s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
                        
						if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fclose", my_argv[1], FALSE, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
					
                        switch(fClose(&boot_sector, argC, my_argv[1])) {
                            case 0:
                                printf("fclose success, \"%s\" has been closed\n", my_argv[1]);                           
                                break;
                            case 1:
                                puts("\nUsage: fclose <filename>\n");
                                break;
                            case 2:
                                printf("ERROR: fclose failed, \"%s\" not found\n", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: fclose failed, \"%s\" has never been opened\n", my_argv[1]);
                                break;
                            case 4:
                                printf("ERROR: fclose failed, \"%s\" is currently closed\n", my_argv[1]);
                                break;
                        }
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fwrite") == 0) //FWRITE COMMAND
                    {
                     
                     
                         int firstByte;
                         int lastByte;
                         if(argC < 4) {
                            puts("\nUsage: fwrite <filename> <position> <data>\n");
                            printf("\n%s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
						 
						 if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fwrite", my_argv[1], FALSE, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
						 
                         int startData = strlen(my_argv[0]) + strlen(my_argv[1]) + strlen(my_argv[2]);
                         startData += 3; //the spaces
                         int totalLen = length;
                         int dataSize = totalLen - startData;
                         char data[dataSize + 1];
                         int x;
                         for(x = 0; x < dataSize; x++) {
                             data[x] = inputBuffer2[startData + x];
                         }
                         data[dataSize] = '\0';
                 
                       
                        if(DEBUG == TRUE) printf("inputBuffer: %s, startdata: %d, totalLen: %d, dataSize: %d, data: %s",
                                                inputBuffer2, startData, totalLen, dataSize, data);
                        /* success code: 0 if fwrite succeeded
                         * error code 1: filename is a directory
                         * error code 2: filename was never opened
                         * error code 3: filename is not currently open
                         * error code 4: no data to be written
                         * error code 5: not open for writing
                         * error code 6: filename does not exist in pwd
                         */ 
                        switch(fWrite(&boot_sector, my_argv[1], my_argv[2], data)) {
                            case 0:
                                firstByte = atoi(my_argv[2]);
                                lastByte = firstByte + strlen(data);
                                printf("fwrite success, wrote \"%s\" to bytes %d-%d of \n", data, firstByte , lastByte , my_argv[1]);                           
                                break;
                            case 1:
                                printf("ERROR: fwrite failed, \"%s\" is a directory\n", my_argv[1]);;
                                break;
                            case 2:
                                printf("ERROR: fwrite failed, \"%s\" has never been opened\n", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: fwrite failed, \"%s\" is not open\n", my_argv[1]);
                                break;
                            case 4:
                                printf("ERROR: fwrite failed on \"%s\" no data to be written\n", my_argv[1]);
                                break;
                            case 5:
                                printf("\nERROR: fwrite failed, \"%s\" is not open for writing \n", my_argv[1]);
                                break;
                            case 6:
                                printf("\nERROR: fwrite failed, \"%s\" does not exist in pwd \n", my_argv[1]);
                                break;
                            case 7:
                                printf("\nERROR: fwrite failed, \"%s\" could not be allocated space \n", my_argv[1]);
                                break;
                            case 8:
                                printf("\nERROR: fwrite succeeded, but the file size of \"%s\" was not updated \n", my_argv[1]);
                                break;
                        }
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fread") == 0) //TESTING TOOL
                    {
                        if(argC < 4) {
                            puts("\nUsage: fread <filename> <position> <number of bytes>\n");
                            printf("\n%s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                        }
                        
                        if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fread", my_argv[1], FALSE, exitCode);
							 printf("\n%s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
                             
                        /* success code: 0 if fwrite succeeded
                         * error code 1: filename is a directory
                         * error code 2: filename was never opened
                         * error code 3: filename is not currently open
                         * error code 4: position is greater than filesize
                         * error code 5: not open for reading
                         * error code 6: filename does not exist in pwd
                         */ 
                        int numBytes;
                        uint32_t actualBytesRead;
                        //fRead(struct BS_BPB * bs, const char * filename, const char * position, const char * numberOfBytes)
                        switch(fRead(&boot_sector, my_argv[1], my_argv[2], my_argv[3], &actualBytesRead)) {
                            case 0:
                                numBytes = atoi(my_argv[3]); //if file is smaller, only report
                                if(numBytes >= actualBytesRead)//we read the filesize bytes
                                    numBytes = actualBytesRead;
                                printf("\nfread success, read %d bytes from \"%s\"\n", numBytes, my_argv[1]);                           
                                break;
                            case 1:
                                printf("ERROR: fread failed, \"%s\" is a directory\n", my_argv[1]);;
                                break;
                            case 2:
                                printf("ERROR: fread failed, \"%s\" has never been opened\n", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: fread failed, \"%s\" is not open\n", my_argv[1]);
                                break;
                            case 4:
                                printf("ERROR: fread failed on \"%s\" position > filesize\n", my_argv[1]);
                                break;
                            case 5:
                                printf("\nERROR: fread failed, \"%s\" is not open for reading \n", my_argv[1]);
                                break;
                            case 6:
                                printf("\nERROR: fread failed, \"%s\" does not exist in pwd \n", my_argv[1]);
                                break;
                            }
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "nextopen") == 0) //MY TESTING TOOL
                    {   int result;
                        //returns the address of the first available entry spot for this chain
                        if( (result = FAT_findNextOpenEntry(&boot_sector, environment.pwd_cluster)) == -1)
                            puts("This cluster is currently full\n");
                        else
                            printf("Entry %d is free in %s\n", result, environment.pwd);
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "last") == 0) //MY TESTING TOOL
                    {
                        /* use this to find the end of a chain
                        *
                        * */
                        if(argC == 2)
                            printf("%d afterreturn %08x\n", getLastClusterInChain(&boot_sector, atoi(my_argv[1])), getLastClusterInChain(&boot_sector, atoi(my_argv[1]))); 
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "free") == 0) //MY TESTING TOOL
                    {

                        /* pass pass this a cluster number and free a cluster chain, will destroy data /* WARNING */
                          
                        if(argC == 2)
                           FAT_freeClusterChain(&boot_sector, atoi(my_argv[1])); 
                        showPromptAndClearBuffer(inputBuffer);
                        break;
                    } else {
                    
					printf("Command: \"%s\" Not Recognized\n", my_argv[0]);
					showPromptAndClearBuffer(inputBuffer);
					break;

                }
			   } // END INPUT BLOCK
			   
			//if there isn't a '\n' in the buffer load up inputBuffer with
			//that character
			default: 
                strncat(inputBuffer, &inputChar, 1); 
                break;
        }

    }

    return 0;
}

