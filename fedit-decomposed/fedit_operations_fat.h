#ifndef _FEDIT_OPERATIONS_FAT_H_
#define _FEDIT_OPERATIONS_FAT_H_

/*description: feed this a cluster and it returns an offset, in bytes, where the
 * fat entry for this cluster is locted
 * 
 * use: use the result to seek to the place in the image and read
 * the FAT entry
* */
uint32_t getFatAddressByCluster(struct BS_BPB * bs, uint32_t clusterNum) ;

/*description: finds the location of this clusters FAT entry, reads
 * and returns the entry value
 * 
 * use: use the result to seek to the place in the image and read
 * the FAT entry
* */
uint32_t FAT_getFatEntry(struct BS_BPB * bs, uint32_t clusterNum) ;

/* description: takes a value, newFatVal, and writes it to the destination
 * cluster, 
 */ 
int FAT_writeFatEntry(struct BS_BPB * bs, uint32_t destinationCluster, uint32_t * newFatVal) ;

#endif
