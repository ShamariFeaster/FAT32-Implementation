#ifndef _FEDIT_OPERATIONS_ENTRY_H_
#define _FEDIT_OPERATIONS_ENTRY_H_

/* description: takes a directory entry populates a file descriptor 
 * to be used in the file tables
 * */
struct FILEDESCRIPTOR * makeFileDecriptor(struct DIR_ENTRY * entry, struct FILEDESCRIPTOR * fd) ;

/* description: prints the contents of a directory entry
*/
int showEntry(struct DIR_ENTRY * entry) ;

/* description: takes a cluster number where bunch of directories are and
 * the offst of the directory you want read and it will store that directory
 * info into the variable dir
 */ 
struct DIR_ENTRY * readEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t clusterNum, int offset);

/* description: takes a cluster number and offset where you want an entry written and write to that 
 * position
 */ 
struct DIR_ENTRY * writeEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t clusterNum, int offset);


/* description: takes a directory cluster and give you the byte address of the entry at
 * the offset provided. used to write dot entries. POSSIBLY REDUNDANT
 */ 
uint32_t byteOffsetofDirectoryEntry(struct BS_BPB * bs, uint32_t clusterNum, int offset);

/* descriptioin: takes a FILEDESCRIPTOR and checks if the entry it was
 * created from is empty. Helper function
 */ 
bool isEntryEmpty(struct FILEDESCRIPTOR * fd) ;

/* description: this will write a new entry to <destinationCluster>. if that cluster
 * is full it grows the cluster chain and write the entry in the first spot
 * of the new cluster. if <isDotEntries> is set this automatically writes both
 * '.' and '..' to offsets 0 and 1 of <destinationCluster>
 * 
 */ 
int writeFileEntry(struct BS_BPB * bs, struct DIR_ENTRY * entry, uint32_t destinationCluster, bool isDotEntries) ;

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
            bool emptyDirectory) ;

/* description: take two entries and cluster info and populates	them with 
 * '.' and '..' entriy information. The entries are	ready for writing to 
 * the disk after this. helper function for mkdir()
  */
int makeSpecialDirEntries(struct DIR_ENTRY * dot, 
						struct DIR_ENTRY * dotDot,
						uint32_t newlyAllocatedCluster,
						uint32_t pwdCluster ) ;
						

/* returns offset of the entry, <searchName> in the pwd 
 * if found return the offset, if not return -1 
 */ 
int getEntryOffset(struct BS_BPB * bs, const char * searchName) ;						

/* description: walks a directory cluster chain looking for empty entries, if it finds one
 * it returns the byte offset of that entry. If it finds none then it returns -1;
 */ 
uint32_t FAT_findNextOpenEntry(struct BS_BPB * bs, uint32_t pwdCluster) ;

#endif
