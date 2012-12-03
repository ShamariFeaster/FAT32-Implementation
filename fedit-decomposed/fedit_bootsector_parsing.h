#ifndef _FEDIT_BOOTSECTOR_PARSING_H_
#define _FEDIT_BOOTSECTOR_PARSING_H_

//-------------END BOOT SECTOR FUNCTIONS -----------------------
/* notes: the alias shown above the functions match up the variables from
 * the Microsoft FAT specificatiions
 */

//alias := rootDirSectors
int rootDirSectorCount(struct BS_BPB * bs) ;

//alias := FirstDataSector
int firstDataSector(struct BS_BPB * bs) ;

//alias := FirstSectorofCluster
uint32_t firstSectorofCluster(struct BS_BPB * bs, uint32_t clusterNum);

/* decription: feed it a cluster and it returns the byte offset
 * of the beginning of that cluster's data
 * */
uint32_t byteOffsetOfCluster(struct BS_BPB *bs, uint32_t clusterNum) ;

//alias := DataSec
int sectorsInDataRegion(struct BS_BPB * bs) ;

//alias := CountofClusters
int countOfClusters(struct BS_BPB * bs) ;

int getFatType(struct BS_BPB * bs) ;

int firstFatTableSector(struct BS_BPB *bs) ;

//the sector of the first clustor
//cluster_begin_lba
int firstClusterSector(struct BS_BPB *bs) ;

/* description: prints info about the image to the screen
 */ 
int showDriveDetails(struct BS_BPB * bs) ;

#endif
