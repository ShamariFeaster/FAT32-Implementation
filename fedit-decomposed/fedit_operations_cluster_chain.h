#ifndef _FEDIT_OPERATIONS_CLUSTER_CHAIN_H_
#define _FEDIT_OPERATIONS_CLUSTER_CHAIN_H_

/* decription: feed this a cluster number that's part of a chain and this
 * traverses it to find the last entry as well as finding the first available
 * free cluster and adds that free cluster to the chain
 */ 
uint32_t FAT_extendClusterChain(struct BS_BPB * bs,  uint32_t clusterChainMember) ;

/* desctipion: pass in a free cluster and this marks it with EOC and sets 
 * the environment.io_writeCluster to the end of newly created cluster 
 * chain
*/
int FAT_allocateClusterChain(struct BS_BPB * bs,  uint32_t clusterNum) ;

/* desctipion: pass in the 1st cluster of a chain and this traverses the 
 * chain and writes FAT_FREE_CLUSTER to every link, thereby freeing the 
 * chain for reallocation
*/
int FAT_freeClusterChain(struct BS_BPB * bs,  uint32_t firstClusterOfChain);

#endif
