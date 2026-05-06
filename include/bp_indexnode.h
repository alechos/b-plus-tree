#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H
#include <record.h>
#include <bf.h>
#include <bp_file.h>
#define INDEX_CAP 100

typedef struct {
    int rec_count;
} BPLUS_INDEX_NODE;

void PrintIndex(BF_Block* block);
int NewIndex(int file_desc,BF_Block* block);
Pair InsertIndex(int file_desc,BPLUS_INFO *bplus_info,BF_Block* block,Pair ovf_pair);
BPLUS_INDEX_NODE GetIndexMetadata(BF_Block* block);


#endif