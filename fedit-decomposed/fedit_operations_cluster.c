#include "fedit.h"
#include "fedit_operations_cluster.h"

/* desctipion: NOT USED but might be in future to clear newly freed clusters
*/
int clearCluster(struct BS_BPB * bs, uint32_t targetCluster) {
	uint8_t clearedCluster[bs->BPB_BytsPerSec];
	//i didn't want to do it this way but compiler wont let me initalize 
	//an array that's sized with a variable :(
	int x;	
	for(x = 0; x < bs->BPB_BytsPerSec; x++) { clearedCluster[x] = 0x00; }
	FILE* f = fopen(environment.imageName, "r+");
    fseek(f, byteOffsetOfCluster(bs, targetCluster), 0);
	fwrite(clearedCluster, 1, bs->BPB_BytsPerSec, f);
	fclose(f);
	return 0;
}

/*description:walks a FAT chain until 0x0fffffff, returns the number of iterations
 * which is the size of the cluster chiain
*/ 
int getFileSizeInClusters(struct BS_BPB * bs, uint32_t firstClusterVal) {
    int size = 1;
    firstClusterVal = (int) FAT_getFatEntry(bs, firstClusterVal);
    //printf("\ngetFileSizeInClusters: %d    ", firstClusterVal);
    while((firstClusterVal = (firstClusterVal & MASK_IGNORE_MSB)) < FAT_EOC) {
       
        size++;
        firstClusterVal = FAT_getFatEntry(bs, firstClusterVal);
        // printf("\ngetFileSizeInClusters: %d    ", firstClusterVal);
    }
    return size;
        
}
/*description: walks a FAT chain until returns the last cluster, where
* the current EOC is. returns the cluster number passed in if it's empty,
* , you should probly call getFirstFreeCluster if you intend to write to
* the FAT to keep things orderly
*/ 
uint32_t getLastClusterInChain(struct BS_BPB * bs, uint32_t firstClusterVal) {
    int size = 1;
    uint32_t lastCluster = firstClusterVal;
    firstClusterVal = (int) FAT_getFatEntry(bs, firstClusterVal);
    //if cluster is empty return cluster number passed in
    if((((firstClusterVal & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER) )
        return lastCluster;
    //mask the 1st 4 msb, they are special and don't count    
    while((firstClusterVal = (firstClusterVal & MASK_IGNORE_MSB)) < FAT_EOC) {
        lastCluster = firstClusterVal;
        //printf("%08x, result: %d\n", firstClusterVal, (firstClusterVal < 0x0FFFFFF8));
        firstClusterVal = FAT_getFatEntry(bs, firstClusterVal);
    }
    return lastCluster;
        
}
/* description: traverses the FAT sequentially and find the first open cluster
 */ 
int FAT_findFirstFreeCluster(struct BS_BPB * bs) {
    int i = 0;
    int totalClusters = countOfClusters(bs);
    while(i < totalClusters) {
        if(((FAT_getFatEntry(bs, i) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER)
            break;
        i++;
    }
    return i;
}
/* decription: returns the total number of open clusters
 */ 
int FAT_findTotalFreeCluster(struct BS_BPB * bs) {
    int i = 0;
    int fatIndex = 0;
    int totalClusters = countOfClusters(bs);
    while(fatIndex < totalClusters) {
        //printf("%08x, %08x, %d \n", (FAT_getFatEntry(bs, fatIndex) & MASK_IGNORE_MSB), ((FAT_getFatEntry(bs, fatIndex) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER), (((FAT_getFatEntry(bs, fatIndex) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER) );
        if((((FAT_getFatEntry(bs, fatIndex) & MASK_IGNORE_MSB) | FAT_FREE_CLUSTER) == FAT_FREE_CLUSTER))
            i++;
        fatIndex++;
    }
    //printf("entries traversed %d\n", fatIndex);
    return i;
}



/* description: pass in a entry and this properly formats the
 * "firstCluster" from the 2 byte segments in the file structure
 */
uint32_t buildClusterAddress(struct DIR_ENTRY * entry) {
    uint32_t firstCluster = 0x00000000;
    firstCluster |=  entry->hiCluster[1] << 24;
    firstCluster |=  entry->hiCluster[0] << 16;
    firstCluster |=  entry->loCluster[1] << 8;
    firstCluster |=  entry->loCluster[0];
    return firstCluster;
}

/* description: convert uint32_t to hiCluster and loCluster byte array
 * and stores it into <entry>
 */ 
int deconstructClusterAddress(struct DIR_ENTRY * entry, uint32_t cluster) {
    entry->loCluster[0] = cluster;
    entry->loCluster[1] = cluster >> 8;
    entry->hiCluster[0] = cluster >> 16;
    entry->hiCluster[1] = cluster >> 24;
    return 0;
}

/* description: feed this a cluster that's a member of a chain and this
 * sets your environment variable "io_writeCluster" to the last in that
 * chain. The thinking is this is where you'll write in order to grow
 * that file. before writing to cluster check with FAT_findNextOpenEntry()
 * to see if there's space or you need to extend the chain using FAT_extendClusterChain()
 */ 
int FAT_setIoWriteCluster(struct BS_BPB * bs, uint32_t clusterChainMember) {
     environment.io_writeCluster = getLastClusterInChain(bs, clusterChainMember);
	 return 0;
}