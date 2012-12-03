#include "fedit.h"
#include "fedit_operations_entry.h"

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
    //printf("readEntry-> add: %d\n", dataAddress);
    FILE* f = fopen(environment.imageName, "r+");
    fseek(f, dataAddress + offset, 0);
	fwrite(entry, 1, sizeof(struct DIR_ENTRY), f);
    printf("writeEntry-> address: %d...Calling showEntry()\n", dataAddress);
    showEntry(entry);
    fclose(f);
    return entry;
}

/* description: takes a directory cluster and give you the byte address of the entry at
 * the offset provided. used to write dot entries. POSSIBLY REDUNDANT
 */ 
uint32_t byteOffsetofDirectoryEntry(struct BS_BPB * bs, uint32_t clusterNum, int offset) {
    printf("\nbyteOffsetofDirectoryEntry: passed in offset->%d\n", offset);
    offset *= 32;
    printf("\nbyteOffsetofDirectoryEntry: offset*32->%d\n", offset);
    uint32_t dataAddress = byteOffsetOfCluster(bs, clusterNum);
    printf("\nbyteOffsetofDirectoryEntry: clusterNum: %d, offset: %d, returning: %d\n", clusterNum, offset, (dataAddress + offset));
    return (dataAddress + offset);
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
            //printf("\ncluster num: %d\n", currentCluster);
            makeFileDecriptor(&entry, &fd);
            //printf("\ngetEntryOffset: offset->%d, searchName->%s, currName->%s\n", offset, searchName, fd.fullFilename);
            if(strcmp(searchName, fd.fullFilename) == 0) {
            
                return offset;
            }
                
       
      
        }
        currentCluster = FAT_getFatEntry(bs, currentCluster); 
   
    }
     return -1;
}

/* description: walks a directory cluster chain looking for empty entries, if it finds one
 * it returns the byte offset of that entry. If it finds none then it returns -1;
 */ 
uint32_t FAT_findNextOpenEntry(struct BS_BPB * bs, uint32_t pwdCluster) {
     struct DIR_ENTRY dir;
    struct FILEDESCRIPTOR fd;
    int dirSizeInCluster = getFileSizeInClusters(bs, pwdCluster);
    //printf("dir Size: %d\n", dirSizeInCluster);
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
			printf("\nFAT_findNextOpenEntry: offset->%d\n", offset);
            readEntry(bs, &dir, pwdCluster, offset);
            //printf("\ncluster num: %d\n", pwdCluster);
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