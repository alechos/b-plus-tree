#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include <bp_datanode.h>
#include <stdbool.h>

#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return -1;     \
    }                         \
  }

#define PRINT_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
    }                         \
  }

/*Prints all records in ascending order.
  Gets leftmost data node and follows data node sibling pointers,printing
  its records allong the way.*/

void PrintRecsOrdered(int file_desc,BPLUS_INFO *bplus_info) {
  int block_id,cur_key;
  char *data;
  BF_Block *block;
  BPLUS_DATA_NODE data_metadata;

  if (bplus_info->height == -1) {return;} //no root

  BF_Block_Init(&block);
  PRINT_BF(BF_GetBlock(file_desc,bplus_info->root,block)); //load root  node

  if (bplus_info->height == 0) {  //root is a data node
    PrintData(block);
    PRINT_BF(BF_UnpinBlock(block));
    return;
  }

  //traverse to lefmost data node from root
  for (int i = 0; i < bplus_info->height; i++) {
    data = BF_Block_GetData(block);
    memcpy(&block_id,data,sizeof(int)); //get leftmost block_id
    BF_UnpinBlock(block);
    PRINT_BF(BF_GetBlock(file_desc,block_id,block)); //get left child
  }

  //reached leftmost datanode, print it
  data_metadata = GetDataMetadata(block);
  PrintData(block);
  
  //follow sibling pointers
  while (data_metadata.sibling != -1) {
    PRINT_BF(BF_UnpinBlock(block));
    PRINT_BF(BF_GetBlock(file_desc,data_metadata.sibling,block));
    data_metadata = GetDataMetadata(block);
    PrintData(block);    //print records
  }
  PRINT_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
}

//Creates a new index root and returns its block id
int new_root(int file_desc,int key,int left,int right) {
  int block_id;
  char* data;
  BF_Block* new_index_root;
  BF_Block_Init(&new_index_root);

  //create new index root block
  block_id = NewIndex(file_desc,new_index_root); //Note: Default record count for indexes is always 1
  data = BF_Block_GetData(new_index_root);
  //set the new roots data
  memcpy(data,&left,sizeof(int));
  memcpy(data + sizeof(int),&key,sizeof(int));
  memcpy(data + 2*sizeof(int),&right,sizeof(int));

  //Memory management
  BF_Block_SetDirty(new_index_root);
  PRINT_BF(BF_UnpinBlock(new_index_root));
  BF_Block_Destroy(&new_index_root);
  return block_id;
}

int BP_CreateFile(char *fileName)
{  
  int file_desc;
  char *data;
  BF_Block *block;
  BPLUS_INFO info;
  

  BF_Block_Init(&block);
  CALL_BF(BF_CreateFile(fileName)); //create new b+ tree file
  CALL_BF(BF_OpenFile(fileName,&file_desc));
  CALL_BF(BF_AllocateBlock(file_desc,block)); //Allocate first block in file, the metadata block

  //initialize metadata
  data = BF_Block_GetData(block);
  info.height = -1;
  info.root = -1; 
  info.rec_size = sizeof(Record); 
  //sets data and index blocks capacity to either maximum or a capped value (the order of the tree)
  info.data_capacity = MIN((BF_BLOCK_SIZE - sizeof(BPLUS_DATA_NODE))/info.rec_size,DATA_CAP);
  info.index_capacity = MIN((BF_BLOCK_SIZE - sizeof(BPLUS_INDEX_NODE))/(sizeof(int)*2),INDEX_CAP);

  //write metadata to fist block
  memcpy(data, &info, sizeof(BPLUS_INFO));

  //memory management
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  CALL_BF(BF_CloseFile(file_desc));
  return 0;
}

BPLUS_INFO* BP_OpenFile(char *fileName, int *file_desc)
{
  BF_Block *block;
  BPLUS_INFO *info;
  char *data;
  //Open file and get it;s metadata
  PRINT_BF(BF_OpenFile(fileName, file_desc));
  BF_Block_Init(&block);
  PRINT_BF(BF_GetBlock(*file_desc, 0, block));

  data = BF_Block_GetData(block);
  info = malloc(sizeof(BPLUS_INFO));  //allocate heap memory to load metadata in

  if(!info){
    fprintf(stderr, "Memory allocation failed for BPLUS_INFO\n");
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    BF_CloseFile(*file_desc);
    return NULL;
  }

  memcpy(info, data, sizeof(BPLUS_INFO));
  PRINT_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  return info;

}

int BP_CloseFile(int file_desc,BPLUS_INFO* info)
{  
  BF_Block *block;
  char *data;

  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(file_desc, 0, block)); //Get metadata block
  data = BF_Block_GetData(block); 
  memcpy(data, info, sizeof(BPLUS_INFO)); //Write metadata in metadata block
  BF_Block_SetDirty(block);
  
  //Memory management
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  free(info);
  CALL_BF(BF_CloseFile(file_desc));
  return 0;
}

Pair insert_recurse(int file_desc,int block,BPLUS_INFO *bplus_info,int *result,Record record,int h) {
  int key,pos,block_id,cur_key;
  char *data, *cur_data;
  Pair pair;
  Record* current_rec;
  BF_Block *current_block,*leaf;
  BPLUS_INDEX_NODE index_metadata;

  //initialize block structs
  BF_Block_Init(&current_block);
  BF_Block_Init(&leaf);

  //load current index node block and get metadata
  PRINT_BF(BF_GetBlock(file_desc,block,current_block));
  index_metadata = GetIndexMetadata(current_block);

  data = BF_Block_GetData(current_block);

  key = record.id; 
  pos = 0;

  //find child pos to recurse into

  while (pos < index_metadata.rec_count ) {
    cur_data = data + sizeof(int) + pos*2*sizeof(int);
    memcpy(&cur_key,cur_data,sizeof(int)); //get key of current record

    if (cur_key > key) {
      break;
    }
    pos++;
  }  
 
  data = data + pos*2*sizeof(int); //data points to id of block about to be recursed
  memcpy(&block_id,data,sizeof(int));

  if (h == bplus_info->height - 1) {  //if child is a leaf node ie a data node block
    PRINT_BF(BF_GetBlock(file_desc,block_id,leaf));
    pair = InsertData(file_desc,bplus_info,leaf,record); //insert record into node
    *result = (pair.block != -1 && record.id > pair.key) ? pair.block : block_id;
    PRINT_BF(BF_UnpinBlock(leaf));
  } else {
    pair = insert_recurse(file_desc,block_id,bplus_info,result,record,h+1); //recurse into child index
  }

  if (pair.block != -1 ) { //if child block split
    pair = InsertIndex(file_desc,bplus_info,current_block,pair); //insert key-id pair in current index node
  
  }

  //memory management
  PRINT_BF(BF_UnpinBlock(current_block));
  BF_Block_Destroy(&current_block);
  BF_Block_Destroy(&leaf);

  return pair;
}

int BP_InsertEntry(int file_desc,BPLUS_INFO *bplus_info, Record record)
{ 
    Pair pair;  
    BF_Block *block;
    int result,absent;
    
    Record tmpRec;  //Temporaty record required by BP_GetEntry
    Record* rec_ptr =&tmpRec;
    absent = BP_GetEntry( file_desc,bplus_info,record.id,&rec_ptr);
    
    if (!absent) {  //if a record with the same id exists, abort
      return -1;
   }

    BF_Block_Init(&block);
    int h = bplus_info->height;
    int root = bplus_info->root;

    if (h == -1) {  //if no root exists
      //make a new data node and set it as root
      root = NewData(file_desc,block); 
      InsertData(file_desc,bplus_info,block,record);
      bplus_info->root = root;
      bplus_info->height = 0;
      result = root;
      PRINT_BF(BF_UnpinBlock(block));

    } else if (h == 0) { //if only a data node exists and its the root

      BF_GetBlock(file_desc,root,block);
      pair = InsertData(file_desc,bplus_info,block,record); //insert record into data node
      result = root;

      if (pair.block != -1) { //if data node split
        //create new index node and set it as the new root
        root = new_root(file_desc,pair.key,root,pair.block); 
        bplus_info->root = root;
        bplus_info->height = 1;
      }

      PRINT_BF(BF_UnpinBlock(block));
    } else { //root is an index node

      //insert record recursively downwards
      pair = insert_recurse(file_desc,root,bplus_info,&result,record,0);

      if (pair.block != -1) { //if the root splits
        //create and set new root
        root = new_root(file_desc,pair.key,root,pair.block); 
        bplus_info->root = root;
        bplus_info->height++;
      }

    }
  
  BF_Block_Destroy(&block);
  return result;
}

int search_recurse(int file_desc,int block,BPLUS_INFO *bplus_info,int value,Record **record,int height) {
  int pos,block_id,cur_key;
  int result = -1;
  char *data, *cur_data;
  BF_Block *current_block,*leaf;
  BPLUS_INDEX_NODE index_metadata;

  //initialize block struct
  BF_Block_Init(&current_block);
  BF_Block_Init(&leaf);

  //load current index node block and get metadata
  PRINT_BF(BF_GetBlock(file_desc,block,current_block));
  index_metadata = GetIndexMetadata(current_block);
  data = BF_Block_GetData(current_block);
  //printf("Block ID: [%d]---->",block);
  //PrintIndex(current_block);
  pos = 0;
  //find child pos to recurse into
  while (pos < index_metadata.rec_count ) {
    cur_data = data + sizeof(int) + pos*2*sizeof(int);
    memcpy(&cur_key,cur_data,sizeof(int)); //get key of current record

    if (cur_key > value) {break;}
    pos++;
  }  
 
  data = data + pos*2*sizeof(int); //data points to id of block about to be recursed
  memcpy(&block_id,data,sizeof(int));

  if (height == bplus_info->height - 1) {  //if child is a leaf node ie a data node block
    PRINT_BF(BF_GetBlock(file_desc,block_id,leaf));
    //printf("Leaf Block ID: [%d]\n",block_id);
    //printf("-------------------\n");
    //PrintData(leaf);
    
    result = SearchData(file_desc,bplus_info,leaf,value,record); //insert record into node
    PRINT_BF(BF_UnpinBlock(leaf));

  } else {
    result = search_recurse(file_desc,block_id,bplus_info,value,record,height + 1);
  }
  PRINT_BF(BF_UnpinBlock(current_block));
  BF_Block_Destroy(&current_block);
  BF_Block_Destroy(&leaf);
  return result;
}

int BP_GetEntry(int file_desc,BPLUS_INFO *bplus_info, int value,Record** record) {  
  BF_Block *block;
      
  BF_Block_Init(&block);
  
  int result = -1;
  int h = bplus_info->height;
  int root = bplus_info->root;
  //printf("Nodes visited during traversal: \n");

  if (h == -1) {  //if no root exists
    *record = NULL;
    result = -1;

  } else if (h == 0) { //if only a data node exists and its the root

    PRINT_BF(BF_GetBlock(file_desc,root,block));
    //PrintData(block);
    result = SearchData(file_desc,bplus_info,block,value,record); //search for record in data node
    PRINT_BF(BF_UnpinBlock(block));
  } else {
    result = search_recurse(file_desc,root,bplus_info,value,record,0); //continue search downwards
  }
  
  BF_Block_Destroy(&block);
  return result;
}

