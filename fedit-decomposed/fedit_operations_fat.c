#include "fedit.h"
#include "fedit_operations_fat.h"


uint32_t getFatAddressByCluster(struct BS_BPB * bs, uint32_t clusterNum) {
    uint32_t FATOffset = clusterNum * 4;
    uint32_t ThisFATSecNum = bs->BPB_RsvdSecCnt + (FATOffset / bs->BPB_BytsPerSec);
    uint32_t ThisFATEntOffset = FATOffset % bs->BPB_BytsPerSec;
    //printf("getFatAddressByCluster: %d", (ThisFATSecNum * bs->BPB_BytsPerSec + ThisFATEntOffset));
    return (ThisFATSecNum * bs->BPB_BytsPerSec + ThisFATEntOffset); 
}


uint32_t FAT_getFatEntry(struct BS_BPB * bs, uint32_t clusterNum) {
    
   // printf("sectorAddress %d\n", sectorAddress);
    FILE* f = fopen(environment.imageName, "r");
    uint8_t aFatEntry[FAT_ENTRY_SIZE];
    uint32_t FATOffset = clusterNum * 4;
    fseek(f, getFatAddressByCluster(bs, clusterNum), 0);
    fread(aFatEntry, 1, FAT_ENTRY_SIZE, f);
    fclose(f);
    uint32_t fatEntry = 0x00000000;
    //printf("buffer: %x\n",  fatEntry);
    int x;
    for(x = 0; x < 4; x++) {
        fatEntry |= aFatEntry[(FATOffset % FAT_ENTRY_SIZE ) + x] << 8 * x;
       //printf("%08x ", fatEntry);
        
    }
        //printf("%08x ", sector[FATOffset % SECTOR_SIZE]);
    //fatEntry &= 0x0FFFFFFF;
    //printf("cluster: %d, next->%08x\n ",clusterNum, fatEntry);
    
    return fatEntry;
}

int FAT_writeFatEntry(struct BS_BPB * bs, uint32_t destinationCluster, uint32_t * newFatVal) {
    FILE* f = fopen(environment.imageName, "r+");
    fseek(f, getFatAddressByCluster(bs, destinationCluster), 0);
    fwrite(newFatVal, 4, 1, f);
    fclose(f);
    printf("FAT_writeEntry: wrote->%d to cluster %d", *newFatVal, destinationCluster);
    return 0;
}
