#include "fedit.h"
#include "fedit_bootsector_parsing.h"
#include "fedit_constants.h"
#include "fedit_driver_functions.h"
#include "fedit_filetable.h"
#include "fedit_operations_cluster.h"
#include "fedit_operations_cluster_chain.h"
#include "fedit_operations_entry.h"
#include "fedit_operations_fat.h"
#include "fedit_operations_filesystem.h"
#include "fedit_structures.h"

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
    
    
	if(initEnvironment(argv[1], &boot_sector) == 1) {
        printf("\nFATAL ERROR: %s was not found in the pwd\n", environment.imageName);
        return 1;
    }
	
	
    printf("1  - %s:%s> ",argv[1], environment.pwd);
    
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
                        printf("\n18 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    }
                    strcpy(inputBuffer2, inputBuffer);
					//printf("inputBuffer: %s\n", inputBuffer);
					argC = tokenize(inputBuffer, my_argv, " ");
                    //printf("my_argv[0]%s\n", my_argv[0]);

                    if(strcmp(my_argv[0], "exit") == 0) //EXIT COMMAND
                    {
                        inputChar = EOF;
                        break;
                    } else
                    
                    if(strcmp(my_argv[0], "ls") == 0) //LS COMMAND
                    {
                        if(argC > 2) {
                            puts("\nUsage: ls [<path>]\n");
                    //if a path was used
                    } else if(argC == 2) { 
                        uint32_t currentCluster = environment.pwd_cluster;
                        if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE) {
                            //if abs ath
                            if(handleAbsolutePaths(&boot_sector, &currentCluster, my_argv[1], successFilename, failFilename, FALSE, FALSE, FALSE) == TRUE ) {
                                if((exitCode = filterFilename(successFilename, FALSE, TRUE)) != SUCCESS) {
                                     printFilterError("ls", my_argv[1], TRUE, exitCode);
                                     printf("\n2 - %s:%s> ",argv[1], environment.pwd);
                                     bzero(inputBuffer, 100);
                                     break;
                                }
                                ls(&boot_sector, currentCluster, FALSE, successFilename, FALSE, searchFile, FALSE, FALSE);
                            }    
                            else 
                                printf("ERROR: : %s is not a valid directory", failFilename);
                                
                        } else {
                            //looking down a single level
                            
                            if((exitCode = filterFilename(my_argv[1], FALSE, TRUE)) != SUCCESS) {
                                 printFilterError("ls", my_argv[1], TRUE, exitCode);
                                 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
                                 bzero(inputBuffer, 100);
                                 break;
							 }
                            
                            if(searchForFile(&boot_sector, my_argv[1], TRUE, searchFile) == TRUE) 
                                ls(&boot_sector, searchFile->firstCluster, FALSE, my_argv[1], FALSE, searchFile, FALSE, FALSE);
                            else
                                printf("ERROR: : %s is not a valid directory", my_argv[1]);
                        }
                    //no path was used
                    } else {
                        
							
                            ls(&boot_sector, environment.pwd_cluster, FALSE, environment.pwd, FALSE, searchFile, FALSE, FALSE);
                    }
                        printf("\n2 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else
					
                    if(strcmp(my_argv[0], "info") == 0) //INFO COMMAND
                    {
                        //this displays where to write if adding to the pwd
                        puts("Current Environment\n");
                        printf("pwd: %s\npwd_cluster: %d\nio_writeCluster: %d\nlast_pwd: %s",
                        environment.pwd, environment.pwd_cluster, environment.io_writeCluster, environment.last_pwd);
                        //showDriveDetails(&boot_sector);
                        printf("\n3 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else
					
					if(strcmp(my_argv[0], "show") == 0) //SHOW COMMAND
                    {
                        TBL_printFileTbl(TRUE);
						TBL_printFileTbl(FALSE);
                        printf("\n4 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "size") == 0) //SIZE COMMAND 
                    {
						
                        if(argC != 2) {
							puts("\nUsage: size <filename>\n");
							printf("\n11 - %s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        
                        if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("size", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
						
                        searchOrPrintFileSize(&boot_sector, my_argv[1], FALSE, FALSE, searchFile);
                        printf("\n4 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "rm") == 0) //RM COMMAND 
                    //searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile)
                    {
                        if(argC != 2) {
							puts("\nUsage: rm <filename | path>\n");
							printf("\n11 - %s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        
                        
						if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("rm", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
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
                        
                        
                        printf("\n11 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "rmdir") == 0) //RMDIR COMMAND 
                    //searchOrPrintFileSize(struct BS_BPB * bs, const char * fileName, bool useAsFileSearch, bool searchForDirectory, struct FILEDESCRIPTOR * searchFile)
                    {
                        if(argC != 2) {
							puts("\nUsage: rmdir <filename | path>\n");
							printf("\n12 - %s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}

                        if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("rmdir", my_argv[1], TRUE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
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
                                 printf("rm Success: \"%s\" has been removed", my_argv[1]);
                                break;
                            case 1:
                                printf("ERROR: %s failed, \"%s\" not emptyn", "rmdir", my_argv[1]);
                                break;
                            case 2:
                                printf("ERROR: %s failed, \"%s\" is not a directory", "rmdir", my_argv[1]);
                                break;
                            case 3:
                                printf("ERROR: %s failed, \"%s\" not found", "rmdir", my_argv[1]);
                                break;
                         } 
                        
                        
                        printf("\n12 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else
                    
                    if(strcmp(my_argv[0], "cd") == 0) //CD COMMAND
                    {
						if(argC != 2) {
							puts("\nUsage: cd <directory_name | path>\n");
							printf("\n5 - %s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						}
                        //don't snag path names
						if(isPathName(my_argv[1], PATH_DELIMITER) == FALSE) {
                            if((exitCode = filterFilename(my_argv[1], FALSE, TRUE)) != SUCCESS) {
                                 printFilterError("cd", my_argv[1], TRUE, exitCode);
                                 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
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
                        printf("\n5 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
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
							printf("\n6.3 - %s:%s> ",argv[1], environment.pwd);
							bzero(inputBuffer, 100);
							break;
						
						}

                         uint32_t targetCluster = environment.pwd_cluster;
                        //if there's a path try to resolve it
                         if(isPathName(my_argv[1], PATH_DELIMITER) == TRUE) { 
                            if(handleAbsolutePaths(&boot_sector, &targetCluster, my_argv[1], successFilename, failFilename, FALSE, TRUE, FALSE) == TRUE ) {
                                printf("targetCluster: %d\n", targetCluster);
                                strcpy(my_argv[1], successFilename);
                            } else {
                                printf("ERROR: Directory not found: \"%s\"\n", failFilename);
                                printf("\n6.3 - %s:%s> ",argv[1], environment.pwd);
                                bzero(inputBuffer, 100);
                                break;
                            }
                        }

                        //check if filename is valid
                        if((exitCode = filterFilename(my_argv[1], TRUE, FALSE)) != SUCCESS) {
							 printFilterError(commandName, my_argv[1], isMkdir, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
							 bzero(inputBuffer, 100);
							 break;
							 }
                             
                        strcpy(fullFilename, my_argv[1]);
                        tokenCnt = tokenize(my_argv[1], tokens, ".");
                        
                        if(isMkdir == TRUE) 
                        {
                            if(searchForFile(&boot_sector, fullFilename, isMkdir, searchFile) == TRUE) {
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
                            if(searchForFile(&boot_sector, fullFilename, isMkdir, searchFile) == TRUE) {
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
                        
                    
                      
                        printf("\n6.3 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fopen") == 0) //FOPEN COMMAND
                    {
                        char modeName[21];
						
                        if(argC < 2) {
                            puts("\nUsage: fopen <filename> <r | w | x>\n");
                            printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
                        
						if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fopen", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
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
                        
                        printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                        
                        
                    } else 
                    
                    if(strcmp(my_argv[0], "fclose") == 0) //FCLOSE COMMAND
                    {
                        if(argC != 2) {
                            puts("\nUsage: fclose <filename>\n");
                            printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
                        
						if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fclose", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
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
                        printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fwrite") == 0) //FWRITE COMMAND
                    {
                     
                     
                         int firstByte;
                         int lastByte;
                         if(argC < 4) {
                            puts("\nUsage: fwrite <filename> <position> <data>\n");
                            printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                            bzero(inputBuffer, 100);
                            break;
                         }
						 
						 if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fwrite", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
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
                 
                       
                        printf("inputBuffer: %s, startdata: %d, totalLen: %d, dataSize: %d, data: %s",inputBuffer2, startData, totalLen, dataSize, data);
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
                                printf("fwrite success, wrote\"%s\" to bytes %d-%d of \n", data, firstByte , lastByte , my_argv[1]);                           
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
                        printf("\n8 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "fread") == 0) //TESTING TOOL
                    {
                        if(argC < 4) {
                            puts("\nUsage: fread <filename> <position> <number of bytes>\n");
                        }
                        
                        if((exitCode = filterFilename(my_argv[1], FALSE, FALSE)) != SUCCESS) {
							 printFilterError("fread", my_argv[1], FALSE, exitCode);
							 printf("\n2 - %s:%s> ",argv[1], environment.pwd);
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
                        //fRead(struct BS_BPB * bs, const char * filename, const char * position, const char * numberOfBytes)
                        switch(fRead(&boot_sector, my_argv[1], my_argv[2], my_argv[3])) {
                            case 0:
                                numBytes = atoi(my_argv[3]);
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
                        printf("\n10 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "nextopen") == 0) //MY TESTING TOOL
                    {
                        //returns the address of the first available entry spot for this chain
                        printf("Entry %d is free\n", FAT_findNextOpenEntry(&boot_sector, environment.pwd_cluster));
                        printf("\n8.5 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "last") == 0) //MY TESTING TOOL
                    {
                        /* use this to find the end of a chain
                        *
                        * */
                        if(argC == 2)
                            printf("%d afterreturn %08x\n", getLastClusterInChain(&boot_sector, atoi(my_argv[1])), getLastClusterInChain(&boot_sector, atoi(my_argv[1]))); 
                        printf("\n9 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else 
                    
                    if(strcmp(my_argv[0], "free") == 0) //MY TESTING TOOL
                    {

                        /* pass pass this a cluster number and free a cluster chain, will destroy data /* WARNING */
                          
                        if(argC == 2)
                           FAT_freeClusterChain(&boot_sector, atoi(my_argv[1])); 
                        printf("\n10 - %s:%s> ",argv[1], environment.pwd);
                        bzero(inputBuffer, 100);
                        break;
                    } else {
                    
					printf("Command: \"%s\" Not Recognized\n", my_argv[0]);
					printf("\nX - %s:%s> ",argv[1], environment.pwd);
					bzero(inputBuffer, 100);
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

