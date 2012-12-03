#include "fedit.h"
#include "fedit_bootsector_parsing.h"

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

/* description: prints info about the image to the screen
 */ 
int showDriveDetails(struct BS_BPB * bs) {

     puts("\nBoot Sector Info:\n");
     /*
    printf("First jmpBoot value (must be 0xEB or 0xE9): 0x%x\n",
           bs->BS_jmpBoot[0]);
	printf("OEM Name: %s\n", bs->BS_OEMName);
    * printf("Sectors occupied by one FAT: %d\n", bs->BPB_FATSz32);
	printf("Number of  Root Dirs: %d\n", bs->BPB_RootEntCnt);
	printf("Total Number of Sectors: %d\n", bs->BPB_TotSec32);
    printf("Fat Type: %d\n\n", getFatType(bs));
    printf("Data Sector size (kB): %d\n", (sectorsInDataRegion(bs) * 512) / 1024);
    * printf("Number of Reserved Sectors: %d\n", bs->BPB_RsvdSecCnt);
    * puts("More Info:\n");
    printf("Number of Data sectors: %d\n", sectorsInDataRegion(bs));
    printf("Root Dir 1st Cluster: %d\n", bs->BPB_RootClus);
	printf("1st data sector: %d\n", firstDataSector(bs));
	printf("1st Sector of the Root Dir: %d\n\n ", firstSectorofCluster(bs, bs->BPB_RootClus));
    * */
	printf("Bytes Per Sector (block size):%d\n", bs->BPB_BytsPerSec);
	printf("Sectors per Cluster: %d\n", bs->BPB_SecPerClus);
    printf("Total clusters: %d\n", countOfClusters(bs));
	printf("Number of FATs: %d\n", bs->BPB_NumFATs);
    printf("Sectors occupied by one FAT: %d\nComputing Free Sectors.......\n", bs->BPB_FATSz32);
    //printf("Number of free sectors: %d\n", FAT_findTotalFreeCluster(bs));
    //printf("First free sector: %d\n", FAT_findFirstFreeCluster(bs));

    
    
    return 0;
}
