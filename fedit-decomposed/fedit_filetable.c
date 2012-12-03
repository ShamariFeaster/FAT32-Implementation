#include "fedit.h"
#include "fedit_filetable.h"

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
			printf("adding %s\n", environment.dirStatsTbl[environment.tbl_dirStatsIndex % TBL_DIR_STATS].filename);
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
	printf("environment.tbl_dirStatsSize: %d\n", environment.tbl_dirStatsSize);
	printf("environment.tbl_openFilesSize: %d\n", environment.tbl_openFilesSize);
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
			printf("searching for %s in table, found %s\n", dirName, environment.dirStatsTbl[x % TBL_DIR_STATS].filename);
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
			//printf("searching for %s in table, found %s\n", dirName, environment.dirStatsTbl[x % TBL_DIR_STATS].filename);
					if(strcmp(environment.dirStatsTbl[x % TBL_DIR_STATS].fullFilename, filename) == 0) {
                        *index = x;
                        return TRUE;
                    }
                }
            
        } else
            return FALSE;
    } else {
        //printf("openFileSize: %d", environment.tbl_openFilesSize);
        
        if(environment.tbl_openFilesSize > 0) {

			int x;
			for(x = 0; x < environment.tbl_openFilesSize; x++) {
			//printf("searching for %s in table, found %s\n", dirName, environment.dirStatsTbl[x % TBL_DIR_STATS].filename);
               
					if(strcmp(environment.openFilesTbl[x % TBL_OPEN_FILES].fullFilename, filename) == 0) {
                             //printf("accessing [%d] \n", x % TBL_OPEN_FILES);
                        *index = x;
                        return TRUE;
                    }
                }
            
        } else
            return FALSE;
    }

    return FALSE;
}