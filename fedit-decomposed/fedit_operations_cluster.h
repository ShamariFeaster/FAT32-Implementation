#ifndef _FEDIT_OPERATIONS_CLUSTER_H_
#define _FEDIT_OPERATIONS_CLUSTER_H_

/* desctipion: NOT USED but might be in future to clear newly freed clusters
*/
int clearCluster(struct BS_BPB * bs, uint32_t targetCluster) ;

/*description:walks a FAT chain until 0x0fffffff, returns the number of iterations
 * which is the size of the cluster chiain
*/ 
int getFileSizeInClusters(struct BS_BPB * bs, uint32_t firstClusterVal) ;

/*description: walks a FAT chain until returns the last cluster, where
* the current EOC is. returns the cluster number passed in if it's empty,
* , you should probly call getFirstFreeCluster if you intend to write to
* the FAT to keep things orderly
*/ 
uint32_t getLastClusterInChain(struct BS_BPB * bs, uint32_t firstClusterVal) ;

/* description: traverses the FAT sequentially and find the first open cluster
 */ 
int FAT_findFirstFreeCluster(struct BS_BPB * bs) ;

/* decription: returns the total number of open clusters
 */ 
int FAT_findTotalFreeCluster(struct BS_BPB * bs) ;

/* description: pass in a entry and this properly formats the
 * "firstCluster" from the 2 byte segments in the file structure
 */
uint32_t buildClusterAddress(struct DIR_ENTRY * entry) ;

/* description: convert uint32_t to hiCluster and loCluster byte array
 * and stores it into <entry>
 */ 
int deconstructClusterAddress(struct DIR_ENTRY * entry, uint32_t cluster) ;

/*  ----------------------------DEPRECATED -------------------------------
 * description: feed this a cluster that's a member of a chain and this
 * sets your environment variable "io_writeCluster" to the last in that
 * chain. The thinking is this is where you'll write in order to grow
 * that file. before writing to cluster check with FAT_findNextOpenEntry()
 * to see if there's space or you need to extend the chain using FAT_extendClusterChain()
 *                
 */ 
int FAT_setIoWriteCluster(struct BS_BPB * bs, uint32_t clusterChainMember) ;

#endif
