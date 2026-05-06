#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bp_datanode.h"
#include "bf.h"
#include "bp_file.h"
#include "record.h"


/*Get Data node's metadata
    Returns BPLUS_DATA_NODE*/
BPLUS_DATA_NODE GetDataMetadata(BF_Block* block) {
    BPLUS_DATA_NODE node_metadata;
    char *data;
    data = BF_Block_GetData(block);
    data = data + (BF_BLOCK_SIZE - sizeof(BPLUS_DATA_NODE));
    memcpy(&node_metadata,data,sizeof(BPLUS_DATA_NODE));

    return node_metadata;  
}

/*Set node metadata*/
static void set_metadata(BF_Block* block,BPLUS_DATA_NODE metadata) {
    char *data;
    data = BF_Block_GetData(block);
    data = data + (BF_BLOCK_SIZE - sizeof(BPLUS_DATA_NODE));
    memcpy(data,&metadata,sizeof(BPLUS_DATA_NODE));
}  

/* Shifts all records to the right from position pos*/
static void shift_right(BF_Block* block,int pos) {
    BPLUS_DATA_NODE node_metadata;
    char *data, *last_rec, *curr, *next;
    Record temp;

    data = BF_Block_GetData(block);
    node_metadata = GetDataMetadata(block);

    if (pos != node_metadata.rec_count) {
    //save rightmost record
        last_rec = data + sizeof(Record)*(node_metadata.rec_count - 1);
        memcpy(&temp,last_rec,sizeof(Record));
        
        //shift records to the right
        for(int i = node_metadata.rec_count - 1; i > pos; i--) {
            curr = data + i*(sizeof(Record));
            next = data + (i-1)*(sizeof(Record));
            memcpy(curr,next,sizeof(Record));
    
        }

        //rewrite saved rightmost record
        data = data + (node_metadata.rec_count)*sizeof(Record);
        memcpy(data,&temp,sizeof(Record));
    }
}
/*Prints records of data block.*/
void PrintData(BF_Block* block) {
    BPLUS_DATA_NODE metadata;
    Record rec;
    char *data;

    metadata = GetDataMetadata(block);
    data = BF_Block_GetData(block);

    printf("Data Block: ");
    for(int i = 0; i < metadata.rec_count; i++) {
        memcpy(&rec,data,sizeof(Record));
        data += sizeof(Record);
        printRecord(rec);
    }
    printf("\n");
}

/*Searches for record in data node with id == value.
    Returns 0 if record has been found. Also sets record to the record found*/
int SearchData(int file_desc,BPLUS_INFO *bplus_info,BF_Block *leaf,int value,Record **record) {
    BPLUS_DATA_NODE node_metadata;
    Record cur_rec;
    char *data,*cur_data, *metadata;

    //get node and its metadata
    node_metadata = GetDataMetadata(leaf);    
    data = BF_Block_GetData(leaf);
    //linear search over data node for record with id == value
    int pos = 0;
    while (pos < node_metadata.rec_count ) {
        cur_data = data + pos*(sizeof(Record));
        memcpy(&cur_rec,cur_data,sizeof(Record));

        if (cur_rec.id == value) {
            //derfrence record and set it to the current node
            **record = cur_rec;
            return 0;
        }
        pos++;
    }  
    //No matching record found
    *record = NULL;
    return -1;
}


/*Creates new data node block with default values and returns it's id
  A pointer to the block is also returned through a passed block pointer*/
int NewData(int file_desc,BF_Block* block) {
    BPLUS_DATA_NODE node_metadata;
    int pos;
    BF_AllocateBlock(file_desc,block);

    //set default values for new nodes metadata
    node_metadata.rec_count = 0;
    node_metadata.sibling = -1;
    set_metadata(block,node_metadata);

    BF_GetBlockCounter(file_desc,&pos);
    pos--;
    
    return pos;
}


/* Handles data block splitting 
    Returns key-block_id pair where block_id is the new block 
    from the split and key is the new blocks lowest key*/

Pair DataSplit(int file_desc,BF_Block* block) {
    Pair pair;
    Record rec;
    BF_Block *new_block;
    BPLUS_DATA_NODE metadata;
    char *data, *half;
    int split_pos,range,old_sibling,sibling;

    metadata = GetDataMetadata(block);
    split_pos = (metadata.rec_count + 1)/2;    //position where split happens
    range = metadata.rec_count - split_pos;    //range of data to move to ovf block
    old_sibling = metadata.sibling; //save old sibling

    data = BF_Block_GetData(block);
    half = data + split_pos*(sizeof(Record)); //points to second half of data block
    
    BF_Block_Init(&new_block);   

    // create new block for split
    sibling = NewData(file_desc,new_block);
    data = BF_Block_GetData(new_block);
    memcpy(data,half,range*sizeof(Record));

    //get id of first record in overflow block
    memcpy(&rec,data,sizeof(Record));
    pair.key = rec.id;
    pair.block = sibling;

    metadata.rec_count = split_pos; // count of recrods in original block cut in half (ceil)
    metadata.sibling = sibling;
    set_metadata(block,metadata);

    metadata.rec_count = range; // count of records in new ovf block
    metadata.sibling = old_sibling; 
    set_metadata(new_block,metadata);

    BF_Block_SetDirty(new_block);
    BF_UnpinBlock(new_block); 
    BF_Block_Destroy(&new_block);

    return pair;
}

/* Performs insertion in a data node
   Returns pair of key-block_id values in case a split happened*/
Pair InsertData(int file_desc,BPLUS_INFO *bplus_info,BF_Block* block,Record record) {
    
    Pair pair;
    BPLUS_DATA_NODE node_metadata;
    Record cur_rec;
    char *data,*cur_data, *metadata;

    //set default values for pair structure
    pair.block = -1;
    pair.key = -1;

    //get insertion node and its metadata
    node_metadata = GetDataMetadata(block);    
    data = BF_Block_GetData(block);
    //find position for insertion of record in data node
    int pos = 0;
    while (pos < node_metadata.rec_count ) {
        cur_data = data + pos*(sizeof(Record));
        memcpy(&cur_rec,cur_data,sizeof(Record));

        if (cur_rec.id > record.id) {
            break;
        }
        pos++;
    }  

    data = data + pos*sizeof(Record);
    shift_right(block,pos); // makes space for new inserted record
    memcpy(data,&record,sizeof(Record));    //writes new record
    node_metadata.rec_count++;
    set_metadata(block,node_metadata);

    // In case maximum capacity has been reached, split data node and extract key-block pair
    if (bplus_info->data_capacity == node_metadata.rec_count) {
        pair = DataSplit(file_desc,block); 
    }  

    BF_Block_SetDirty(block);
    return pair;
  
}



