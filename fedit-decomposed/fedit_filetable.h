#ifndef _FEDIT_FILETABLE_H_
#define _FEDIT_FILETABLE_H_

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
								int isOpen) ;
								
/* adds a file object to either the fd table or the dir history table
 */ 
int TBL_addFileToTbl(struct FILEDESCRIPTOR * file, int isDir);

/* description: if <isDir> is set prints the content of the open files table
 * if not prints the open directory table
 */ 
int TBL_printFileTbl(int isDir) ;

/* desctipion searches the open directory table for entry if it finds it,
 * it returns the name of the parent directory, else it returns an empty string
*/
const char * TBL_getParent(const char * dirName) ;

/* description:  "index" is set to the index where the found element resides
 * returns TRUE if file was found in the file table
 * 
 */ 
bool TBL_getFileDescriptor(int * index, const char * filename, bool isDir);

#endif
