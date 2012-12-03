#ifndef _FEDIT_OPERATIONS_FILESYSTEM_H_
#define _FEDIT_OPERATIONS_FILESYSTEM_H_

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

int ls(struct BS_BPB * bs, 
		uint32_t clusterNum, 
		bool cd, 
		const char * directoryName, 
		bool goingUp, 
		struct FILEDESCRIPTOR * searchFile, 
		bool useAsSearch, 
		bool searchForDir) ;


/* description: wrapper for ls. tries to change the pwd to <directoryName> 
 * This doesn't use the print to screen functinoality of ls. if <goingUp> 
 * is set it uses the open directory table to find the pwd's parent
 */ 
int cd(struct BS_BPB * bs, const char * directoryName, int goingUp,  struct FILEDESCRIPTOR * searchFile) ;

/* description: DOES NOT allocate a cluster chain. Writes a directory 
 * entry to <targetDirectoryCluster>. Error handling is done in the driver
 */ 
int touch(struct BS_BPB * bs, const char * filename, const char * extention, uint32_t targetDirectoryCluster) ;

/* description: allocates a new cluster chain to <dirName> and writes the directory
 * entry to <targetDirectoryCluster>, then writes '.' and '..' entries into the first
 * cluster of the newly allocated chain. Error handling is done in the driver
 */ 
int mkdir(struct BS_BPB * bs, const char * dirName, const char * extention, uint32_t targetDirectoryCluster) ;

/*
 * success code 0: file found and removed
 * error code 1: file is open
 * error code 2: file is a directory
 * error code 3: file or directory not found
 * 
 * description: removes a file entry from the <searchDirectoryCluster> if
 * it exists. see error codes for other assumptions
 */ 
int rm(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) ;


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
int rmDir(struct BS_BPB * bs, const char * searchName, uint32_t searchDirectoryCluster) ;

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
int fClose(struct BS_BPB * bs, int argC, const char * filename) ;

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
int fOpen(struct BS_BPB * bs, int argC, const char * filename, const char * option, char * modeName) ;

/* description: takes a cluster number of the first cluster of a file and its size. 
 * this function will write <dataSize> bytes starting at position <pos>. 
 */ 
int FWRITE_writeData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, const char * data, uint32_t dataSize) ;

/* description: takes a cluster number of a file and its size. this function will read <dataSize>
 * bytes starting at position <pos>. It will print <dataSize> bytes to the screen until the end of
 * the file is reached, as determined by <fileSize>
 */ 
int FREAD_readData(struct BS_BPB * bs, uint32_t firstCluster, uint32_t pos, uint32_t dataSize, uint32_t fileSize) ;



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
int fWrite(struct BS_BPB * bs, const char * filename, const char * position, const char * data) ;

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
  * 'th byte
  */ 
int fRead(struct BS_BPB * bs, const char * filename, const char * position, const char * numberOfBytes) ;

/* desctipion: wrapper that uses ls to search a directory for a file and puts its info
 * into searchFile if found. if <useAsFileSearch> is set it disables the printing enabling
 * it to be used as a file search utility. if <searchForDirectory> is set it excludes
 * files from the search
 */ 
bool searchOrPrintFileSize(struct BS_BPB * bs, 
							const char * fileName, 
							bool useAsFileSearch, 
							bool searchForDirectory, 
							struct FILEDESCRIPTOR * searchFile) ;

/* description: wrapper for searchOrPrintFileSize searchs pwd for filename. 
 * returns TRUE if found, if found the resulting entry is put into searchFile
 * for use outside the function. if <searchForDirectory> is set it excludes
 * files from the search
 */ 
bool searchForFile(struct BS_BPB * bs, 
					const char * fileName, 
					bool searchForDirectory, 
					struct FILEDESCRIPTOR * searchFile) ;

#endif
