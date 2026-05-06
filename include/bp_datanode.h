#ifndef BP_DATANODE_H
#define BP_DATANODE_H
#include <record.h>
#include <record.h>
#include <bf.h>
#include <bp_file.h>
#include <bp_indexnode.h>
#define DATA_CAP 100



typedef struct BPLUS_DATA_NODE{
    int rec_count;
    int sibling;
} BPLUS_DATA_NODE;

void PrintData(BF_Block* block);
int SearchData(int file_desc,BPLUS_INFO *bplus_info,BF_Block *leaf,int value,Record **record);
BPLUS_DATA_NODE GetDataMetadata(BF_Block* block);
int NewData(int file_desc,BF_Block* block);
Pair InsertData(int file_desc,BPLUS_INFO *bplus_info,BF_Block *block,Record record);

#endif 