#include "fedit.h"
#include "fedit_operations_cluster_chain.h"

/* decription: feed this a cluster number that's part of a chain and this
 * traverses it to find the last entry as well as finding the first available
 * free cluster and adds that free cluster to the chain
 */ 
uint32_t FAT_extendClusterChain(struct BS_BPB * bs,  uint32_t clusterChainMember) {
    uint32_t firstFreeCluster = FAT_findFirstFreeCluster(bs);
	uint32_t lastClusterinChain = getLastClusterInChain(bs, clusterChainMember);
    //printf("1stfree: %d ", environment.io_writeCluster);
    FAT_writeFatEntry(bs, lastClusterinChain, &firstFreeCluster);
    FAT_writeFatEntry(bs, firstFreeCluster, &FAT_EOC);
    return firstFreeCluster;
}

/* desctipion: pass in a free cluster and this marks it with EOC and sets 
 * the environment.io_writeCluster to the end of newly created cluster 
 * chain
*/
int FAT_allocateClusterChain(struct BS_BPB * bs,  uint32_t clusterNum) {
	//environment.io_writeCluster = clusterNum;
	FAT_writeFatEntry(bs, clusterNum, &FAT_EOC);
	return 0;
}

/* desctipion: pass in the 1st cluster of a chain and this traverses the 
 * chain and writes FAT_FREE_CLUSTER to every link, thereby freeing the 
 * chain for reallocation
*/
int FAT_freeClusterChain(struct BS_BPB * bs,  uint32_t firstClusterOfChain){
    int dirSizeInCluster = getFileSizeInClusters(bs, firstClusterOfChain);
    printf("dir Size: %d\n", dirSizeInCluster);
    int currentCluster= firstClusterOfChain;
    int nextCluster;
    int clusterCount;
    
    for(clusterCount = 0; clusterCount < dirSizeInCluster; clusterCount++) {  
       nextCluster = FAT_getFatEntry(bs, currentCluster); 
       FAT_writeFatEntry(bs, currentCluster, &FAT_FREE_CLUSTER);
       currentCluster = nextCluster;
    }
    return 0; 
    
}