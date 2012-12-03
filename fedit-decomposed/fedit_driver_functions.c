#include "fedit.h"
#include "fedit_driver_functions.h" 

int tokenize(char * argString, char ** argV, const char * delimiter)
{
	int index = 0;
    char * tokens =  strtok(argString, delimiter);
	while(tokens != NULL) {
		argV[index] = tokens;
		//printf("loadArgs: argV[%d] = %s\n", index, tokens);
		tokens = strtok(NULL, delimiter);
		index++;
	}
	
	return index;
}

int isPathName(const char * pathName, const char * delimiter) {
    int pathLength = strlen(pathName);
    int x; 
    for(x = 0; x< pathLength; x++) {
        if(pathName[x] == delimiter[0])
            return TRUE;
    }
    return FALSE;
}

int initEnvironment(const char * imageName, struct BS_BPB * boot_sector) {
    strcpy(environment.imageName, imageName);
	FILE* f = fopen(imageName, "r");
    if(f == NULL) {
        return 1;
    }
    
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
    
	//initalize and allocate open directory table
	environment.dirStatsTbl =  malloc(TBL_DIR_STATS * sizeof(struct FILEDESCRIPTOR));
	int i;
	for (i = 0; i < TBL_DIR_STATS; i++)
		environment.dirStatsTbl[i] = *( (struct FILEDESCRIPTOR *) malloc( sizeof(struct FILEDESCRIPTOR) ));

	//initalize and allocate open files table
	environment.openFilesTbl =  malloc(TBL_OPEN_FILES * sizeof(struct FILEDESCRIPTOR));
	for (i = 0; i < TBL_OPEN_FILES; i++)
		environment.openFilesTbl[i] = *( (struct FILEDESCRIPTOR *) malloc( sizeof(struct FILEDESCRIPTOR) ));

    return 0;
}

int filterFilename(const char * argument, bool isTouchOrMkdir, bool isCdOrLs){
	char filename[200];
	int invalid = 0;
	int periodCnt = 0;
	int tokenCount = 0;
	char * tokens[10];
    int x;
    puts("\nhere1\n");
	strcpy(filename, argument);
	puts("\nhere2\n");
	// '.' and '..' are not allowed with touch or mkdir
	if(isTouchOrMkdir == TRUE && (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0))
		return 5;
		puts("\nhere3\n");
	//cd and ls will allow special directories
	if(isCdOrLs == TRUE && (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0))
		return 0;
	printf("invalid: %d, periodCnt: %d, argument: %s, filename: %s strlen(filename): %d\n", invalid, periodCnt, argument, filename, strlen(filename));
	for(x = 0; x < strlen(filename); x++) {
		if(
			((filename[x] >= ALPHA_NUMBERS_LOWER_BOUND && filename[x] <= ALPHA_NUMBERS_UPPER_BOUND) || 
			(filename[x] >= ALPHA_UPPERCASE_LOWER_BOUND && filename[x] <= ALPHA_UPPERCASE_UPPER_BOUND)) == FALSE) 
			{      
					invalid++;   
					if(filename[x] == PERIOD) 
						periodCnt++;
			} 
	}
    printf("invalid: %d, periodCnt: %d, argument: %s, filename: %s strlen(filename): %d\n", invalid, periodCnt, argument, filename, strlen(filename));
	if(invalid > 1) {//more than 1 invalid character
		//printf("ERROR: \"%s\" not a valid %s name: only alphameric characters allowd\n", filename, fileType);
		return 1;
	} else if(invalid == 1){ //1 invalid that's a period
		if(periodCnt == 1) {
            puts(filename);
			tokenCount = tokenize(filename, tokens, ".");
            puts("\nhere6\n");
			//FIS -> foo. is legal change this to make it so
			if(tokenCount < 2) { //is filename or ext missing?
				//printf("\nERROR: %s failed, %s name or extention not given", commandName, fileType);
				return 2;
			}
            puts("\nhere7\n");
			if(strlen(tokens[0]) > 8) { //is filename > 8?
				//printf("ERROR: %s failed, %s name \"%s\" longer than 8 characters", commandName, fileType, tokens[0] );
				return 3;
			}
            puts("\nhere8\n");
			if(strlen(tokens[1]) > 3) { //is ext < 3
				//printf("ERROR: %s failed, extention \"%s\" longer than 3 characters", commandName, tokens[1]);
				return 4;
			}
		}
	}else { //there's no period
		//is filename > 8?
		if(strlen(filename) > 8) {
			//printf("ERROR: %s failed, %s name \"%s\" longer than 8 characters", commandName, fileType, filename );
			return 3;
		} 
	}
	return 0;
	//re-tokenize once we exit here
}


int printFilterError(const char * commandName, const char * filename, bool isDir, int exitCode) {
	char fileType[11];
    
	if(isDir == TRUE)
		strcpy(fileType, "directory");
	else
		strcpy(fileType, "file");
		
	switch(exitCode) {
		case 1:
			printf("ERROR: \"%s\" not a valid %s name: only uppercase alphameric characters allowed\n", filename, fileType);
			break;
		case 2:
			printf("\nERROR: %s failed on filename %s, name or extention not given", commandName, fileType);
			break;
		case 3:
			printf("ERROR: %s failed, %s name \"%s\" longer than 8 characters", commandName, fileType, filename );
			break;
		case 4:
			printf("ERROR: %s failed, extention \"%s\" longer than 3 characters", commandName, filename);
			break;
		case 5:
			printf("ERROR: %s failed, cannot \"%s\" using \"%s\" as a name", commandName, commandName, filename);
			break;
	
	}
}

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
        //ls(bs, clusterNum,   cd,  directoryName, goingUp, searchFile, useAsSearch, searchForDir)
        if( ls(bs, *targetCluster, TRUE, paths[x], FALSE, searchFile, TRUE, TRUE) == TRUE ) {
            previousCluster = *targetCluster;
            *targetCluster = searchFile->firstCluster;
            //printf("%s \n", paths[x]);
        } else {
            strcpy(failFilename, paths[x]); // pass back to caller
            printf("failed on %s\n", failFilename);
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
        //printf("succedSilename: %s\n", successFilename);
        if(isMkdir == TRUE)
            *targetCluster = previousCluster;
            
        return TRUE;
    
    }
            //ls(&boot_sector, currentCluster, FALSE, paths[pathsArgC - 1], FALSE, searchFile, FALSE, FALSE);
    
    
}
