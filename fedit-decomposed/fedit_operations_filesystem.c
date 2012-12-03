#include "fedit.h"
#include "fedit_operations_filesystem.h"

int ls(struct BS_BPB * bs, uint32_t clusterNum, bool cd, const char * directoryName, bool goingUp, struct FILEDESCRIPTOR * searchFile, bool useAsSearch, bool searchForDir) {
    struct DIR_ENTRY dir;

	int dirSizeInCluster = getFileSizeInClusters(bs, clusterNum);
    //printf("lastClusterInChain: %d\n", getLastClusterInChain(bs, (int) clusterNum));
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
            //printf("\ncluster num: %d\n", clusterNum);
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
                if(useAsSearch == TRUE) {/*
                    if(strlen(searchFile->extention) < 1 && strcmp(searchFile->filename, directoryName) == 0 && searchFile->dir == searchForDir) { <-------CHANGED
                        return TRUE;
                    }
                    if(strlen(searchFile->extention) > 0 && strcmp(searchFile->fullFilename, directoryName) == 0 && searchFile->dir == searchForDir ) {
                        return TRUE;
                    }    */
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
                        //printf("dirStats[%d].parent = %s", environment.tbl_dirStatsIndex, environment.dirStatsTbl[environment.tbl_dirStatsIndex-1]->parent);
                        FAT_setIoWriteCluster(bs, environment.pwd_cluster);
                        return TRUE;
                    }
                    
                    //fetch parent cluster from the directory table
                    if(searchFile->dir == TRUE && strcmp(directoryName, PARENT) == 0 && goingUp == TRUE) {
                        const char * parent = TBL_getParent(environment.pwd);
                        //printf("ls: parent from getParent %s\n", parent );
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

int cd(struct BS_BPB * bs, const char * directoryName, int goingUp,  struct FILEDESCRIPTOR * searchFile) {
   
    return ls(bs, environment.pwd_cluster, TRUE, directoryName, goingUp, searchFile, FALSE, FALSE); 
}


int touch(struct BS_BPB * bs, const char * filename, const char * extention, uint32_t targetDirectoryCluster) {
    struct DIR_ENTRY newFileEntry;
    createEntry(&newFileEntry, filename, extention, FALSE, 0, 0, FALSE, FALSE);
    writeFileEntry(bs, &newFileEntry, targetDirectoryCluster, FALSE);
    return 0;
    }
 
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

int rm(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) {
    struct DIR_ENTRY entry;
    struct FILEDESCRIPTOR fd;
    struct FILEDESCRIPTOR fileTableFd;
    uint32_t currentCluster = searchDirectoryCluster;
    int dirSizeInCluster = getFileSizeInClusters(bs, currentCluster);
    //printf("dir Size: %d\n", dirSizeInCluster);
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
			printf("\rm: offset->%d\n", offset);
            readEntry(bs, &entry, currentCluster, offset);
            //printf("\ncluster num: %d\n", searchDirectoryCluster);
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

int rmDir(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) {
    struct DIR_ENTRY entry;
    struct FILEDESCRIPTOR fd;
    uint32_t currentCluster = searchDirectoryCluster;
    int dirSizeInCluster = getFileSizeInClusters(bs, currentCluster);
    printf("dir Size: %d\n", dirSizeInCluster);
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
			//printf("\rm: offset->%d\n", offset);
            readEntry(bs, &entry, currentCluster, offset);
            printf("\ncluster num: %d\n", searchDirectoryCluster);
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


int FWRITE_writeData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, const char * data, uint32_t dataSize){
            bool debug = TRUE;
            //determine the cluster offset of the cluster
            uint32_t currentCluster = firstCluster;
            // where pos is to start writing data
            uint32_t writeClusterOffset = (pos / bs->BPB_BytsPerSec);
            uint32_t posRelativeToCluster = pos %  bs->BPB_BytsPerSec;
            uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
            uint32_t remainingClustersToWalk = fileSizeInClusters - writeClusterOffset;
            if(debug == TRUE) printf("pos: %d, writeClusterOffset: %d, posRelativeToCluster: %d\n",pos, writeClusterOffset, posRelativeToCluster);
            int x;
            int dataIndex;
            //seek to the proper cluster offset from first cluster
            for(x = 0; x < writeClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            if(debug == TRUE) printf("after seek currentCluster is: %d\n", currentCluster);
            //if here, we just start writing at the last cluster right after
            //the last writen byte
            dataIndex = 0;
			uint32_t dataWriteLength = dataSize;
			uint32_t dataRemaining = dataSize;
			uint32_t fileWriteOffset;
			uint32_t startWritePos;
            FILE* f = fopen(environment.imageName, "r+");
            for(x = 0; x < remainingClustersToWalk; x++) {
                
                startWritePos = byteOffsetOfCluster(bs, currentCluster);
                
                if(x == 0)
                    startWritePos += posRelativeToCluster;
                        
                if(dataRemaining > bs->BPB_BytsPerSec)
                    dataWriteLength = bs->BPB_BytsPerSec;
                else
                    dataWriteLength = dataRemaining;
                if(debug == TRUE) printf("right before write: dataWriteLength:%d\n", dataWriteLength);
                //write data 1 character at a time
                for(fileWriteOffset = 0; fileWriteOffset < dataWriteLength; fileWriteOffset++) {
                    fseek(f, startWritePos + fileWriteOffset, 0);
                    if(debug == TRUE) printf("writing: %c to (startWritePos->%d + fileWriteOffset->) = %d, dataWriteLength: %d, dataRemaining:%d\n",data[dataIndex], startWritePos, fileWriteOffset,(startWritePos + fileWriteOffset),  dataWriteLength, dataRemaining );
                    uint8_t dataChar[1] = {data[dataIndex++]};
                    fwrite(dataChar, 1, 1, f);
                    
                }
                
                dataRemaining -= dataWriteLength;
                if(dataRemaining == 0){
                    if(debug == TRUE) puts("dataRemaining is 0\n");
                    break;                      
                }    
                
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(debug == TRUE) printf("Moving to cluster: %d", currentCluster);
            }
            fclose(f);
			return 0;
}

int FREAD_readData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, uint32_t dataSize, uint32_t fileSize){
            bool debug = FALSE;
            //determine the cluster offset of the cluster
            uint32_t currentCluster = firstCluster;
            // where pos is to start writing data
            uint32_t readClusterOffset = (pos / bs->BPB_BytsPerSec);
            uint32_t posRelativeToCluster = pos %  bs->BPB_BytsPerSec;
            uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
            uint32_t remainingClustersToWalk = fileSizeInClusters - readClusterOffset;
            if(debug == TRUE) printf("fread> pos: %d, readClusterOffset: %d, posRelativeToCluster: %d\n",pos, readClusterOffset, posRelativeToCluster);
            int x;
            //seek to the proper cluster offset from first cluster
            for(x = 0; x < readClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            if(debug == TRUE) printf("fread> after seek currentCluster is: %d\n", currentCluster);
            //if here, we just start writing at the last cluster right after
            //the last writen byte
			uint32_t dataReadLength = dataSize;
			uint32_t dataRemaining = dataSize;
			uint32_t fileReadOffset;
			uint32_t startReadPos;
            FILE* f = fopen(environment.imageName, "r");
            for(x = 0; x < remainingClustersToWalk; x++) {
                
                startReadPos = byteOffsetOfCluster(bs, currentCluster);
                
                if(x == 0)
                    startReadPos += posRelativeToCluster;
                        
                if(dataRemaining > bs->BPB_BytsPerSec)
                    dataReadLength = bs->BPB_BytsPerSec;
                else
                    dataReadLength = dataRemaining;
                if(debug == TRUE) printf("fread> right before write: dataReadLength:%d\n", dataReadLength);
                //write data 1 character at a time
                for(fileReadOffset = 0; fileReadOffset < dataReadLength && (pos + fileReadOffset) < fileSize; fileReadOffset++) {
                    fseek(f, startReadPos + fileReadOffset, 0);
                    if(debug == TRUE) printf("fread> (startReadPos->%d + fileReadOffset->%d) = %d, dataReadLength: %d, dataRemaining:%d\n", startReadPos, fileReadOffset,(startReadPos + fileReadOffset),  dataReadLength, dataRemaining );
                    uint8_t dataChar[1];
                    fread(dataChar, 1, 1, f);
                    printf("%c", dataChar[0]);
                }
                
                dataRemaining -= dataReadLength;
                if(dataRemaining == 0){
                    if(debug == TRUE) puts("fread> dataRemaining is 0\n");
                    break;                      
                }    
                
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(debug == TRUE) printf("fread> Moving to cluster: %d\n", currentCluster);
            }
            fclose(f);
			return 0;
}

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
    
    //if file was touched, no cluster chain was allocated, do it here
    if(searchFile->firstCluster == 0) {
        if( ( offset = getEntryOffset(bs, filename) ) == -1)
            return 7;
        readEntry(bs,  &entry, environment.pwd_cluster, offset);
        
        firstCluster = FAT_findFirstFreeCluster(bs);
        FAT_allocateClusterChain(bs, firstCluster);
        
        deconstructClusterAddress(&entry, firstCluster);
        if(debug == TRUE) printf("Retreiving Entry From Offset %d, First cluster allocated %d", offset, firstCluster);
        environment.openFilesTbl[fileTblIndex].firstCluster = firstCluster;//set firstCluster
        environment.openFilesTbl[fileTblIndex].size = 0;                    // set size, althoug it's probaly set
        writeEntry(bs,  &entry, environment.pwd_cluster, offset);
    } else{
        fileSize = environment.openFilesTbl[fileTblIndex].size;
        firstCluster = currentCluster = environment.openFilesTbl[fileTblIndex].firstCluster;
    }
    //get max chain size for walking the chain
    uint32_t fileSizeInClusters = getFileSizeInClusters(bs, firstCluster);
    
    if(debug == TRUE) printf("fwrite> data:%s, data length:%d, pos:%d, fileSize:%d, firstCluster:%d \n", data, dataSize, pos, fileSize, firstCluster);
    if(debug == TRUE) printf("fwrite> (pos + dataSize):%d,  \n", (pos + dataSize));
    
    if((pos + dataSize) > fileSize) {
        //cluster size must grow
        if(debug == TRUE) puts("case 1\n");
        newSize = pos + dataSize;
        
        totalSectors = newSize / bs->BPB_BytsPerSec;
        if(newSize > 0 && (newSize % bs->BPB_BytsPerSec != 0))
            totalSectors++;
        
        //determine newsize of chain
        additionalsectors = totalSectors - fileSizeInClusters;
        
        //grow the chain to new necessary size
        for(x = 0; x < additionalsectors; x++) {
            if(debug == TRUE) printf("\nExtending chain: %d\n", x);
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
       if(debug == TRUE) printf("totalSectors: %d,additionalsectors: %d,fileSizeInClusters: %d,paddingLength: %d, usedBytesInLastSector: %d,openBytesInLastSector: %d\n",totalSectors,additionalsectors,fileSizeInClusters,paddingLength, usedBytesInLastSector,openBytesInLastSector);
        if(paddingLength > 0) {
           if(debug == TRUE) puts("padding \n");
            //determine the cluster offset of the last cluster
            //to start padding 
            paddingClusterOffset = (fileSize / bs->BPB_BytsPerSec);
            
            //set remaining number of clusters to be walked til EOC
            remainingClustersToWalk = fileSizeInClusters - paddingClusterOffset;
            
            //seek to the proper cluster offset
            for(x = 0; x < paddingClusterOffset; x++)
                currentCluster = FAT_getFatEntry(bs, currentCluster);
            
            uint8_t padding[1] = {0x20};
            if(debug == TRUE) printf("paddingClusterOffset: %d, remainingClustersToWalk: %d,currentCluster: %d\n",paddingClusterOffset, remainingClustersToWalk,currentCluster);
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
                //printf("x: %d,startWritePos: %d, paddingLength: %d, paddingWriteLength: %d,paddingRemaining: %d\n",x,startWritePos,paddingLength,paddingWriteLength,paddingRemaining);
                //write padding
                for(fileWriteOffset = 0; fileWriteOffset < paddingWriteLength ; fileWriteOffset++) {
                    fseek(f, startWritePos + fileWriteOffset, 0);
                    fwrite(padding, 1, 1, f);
                    //printf("writing pad> fileWriteOffset: %d,(startWritePos + fileWriteOffset): %d,paddingWriteLength: %d \n",fileWriteOffset,(startWritePos + fileWriteOffset), paddingWriteLength );
                    //~ if(fileWriteOffset > 10)
                        //~ break;
                    
                }
                
                paddingRemaining -= paddingWriteLength;
                if(paddingRemaining == 0) {
                    if(debug == TRUE) puts("paddingRemaining is 0\n");
                    break;                      
                }
                currentCluster = FAT_getFatEntry(bs, currentCluster); 
                if(debug == TRUE) printf("Moving tp cluster: %d", currentCluster);
            }  
        } //end padding if
        
        if(debug == TRUE) puts("Starting Data Write From Case 1\n");
        FWRITE_writeData(bs, firstCluster, pos, data, dataSize);

    } else {
        //cluster size will not grow
        newSize = fileSize;
        if(debug == TRUE) puts("Starting Data Write From Case 2\n");
        FWRITE_writeData(bs, firstCluster, pos, data, dataSize);
    }
    
    
        
    //update filesize
    
    if(fileSize != newSize) {
        if( ( offset = getEntryOffset(bs, filename) ) == -1)
            return 8;
        readEntry(bs,  &entry, environment.pwd_cluster, offset);
        entry.fileSize = newSize;
        if(debug == TRUE) printf("Retreiving Entry From Offset %d, filesize is now ", offset, entry.fileSize);
        environment.openFilesTbl[fileTblIndex].size = newSize;
        writeEntry(bs,  &entry, environment.pwd_cluster, offset);
    }
    fclose(f);
    return 0;
    
}


int fRead(struct BS_BPB * bs, const char * filename, const char * position, const char * numberOfBytes) {
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
    
    fileSize = environment.openFilesTbl[fileTblIndex].size;
    firstCluster = currentCluster = environment.openFilesTbl[fileTblIndex].firstCluster;
    FREAD_readData(bs, firstCluster, pos, dataSize, fileSize);

    fclose(f);
    return 0;
}


bool searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile) {
    if(ls(bs, environment.pwd_cluster, TRUE, fileName, FALSE, searchFile, TRUE, searchForDirectory) == TRUE) {
        if(useAsFileSearch == FALSE)
            printf("File: %s is %d byte in size", fileName ,searchFile->size);
        return TRUE;
    } else {
        if(useAsFileSearch == FALSE)
            printf("ERROR: File: %s was not found", fileName);
        return FALSE;
    }
}


bool searchForFile(struct BS_BPB * bs, const char * fileName, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile) {
    return searchOrPrintFileSize(bs, fileName, TRUE, searchForDirectory, searchFile);
} 