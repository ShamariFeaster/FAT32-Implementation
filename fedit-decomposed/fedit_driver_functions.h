#ifndef _FEDIT_DRIVER_FUNCTIONS_H_
#define _FEDIT_DRIVER_FUNCTIONS_H_

int tokenize(char * argString, char ** argV, const char * delimiter) ;

/* description: if a path character exists mark this as a path
 * <delimiter> is a single character string
 */ 
int isPathName(const char * pathName, const char * delimiter) ;

/* loads up boot sector and initalizes file and directory tables
*/
int initEnvironment(const char * imageName, struct BS_BPB * boot_sector) ;

/*
* error code 1: too many invalid characters
* error code 2: extention given but no filename
* error code 3: filename longer than 8 characters
* error code 4: extension longer than 3 characters
* error code 5: cannot touch or mkdir special directories
* success code 0: filename is valid
*/

int filterFilename(const char * argument, bool isTouchOrMkdir, bool isCdOrLs);

int printFilterError(const char * commandName, const char * filename, bool isDir, int exitCode);

bool handleAbsolutePaths(struct BS_BPB * bs, 
							uint32_t * targetCluster, 
							char * path, 
							char * successFilename, 
							char * failFilename, 
							bool isCd, 
							bool searchForFile, 
							bool isMkdir);

#endif
