#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bp_indexnode.h"
#include "bf.h"
#include "bp_file.h"
#include "record.h"


BPLUS_INDEX_NODE GetIndexMetadata(BF_Block* block) {
    BPLUS_INDEX_NODE node_metadata;
    Record* current_rec;
    char *data;
    data = BF_Block_GetData(block);
    data = data + BF_BLOCK_SIZE - sizeof(BPLUS_INDEX_NODE);
    memcpy(&node_metadata,data,sizeof(BPLUS_INDEX_NODE));

    return node_metadata;  
}

/*Set data block metadata*/
void set_metadata(BF_Block* block,BPLUS_INDEX_NODE metadata) {
    char *data;

    data = BF_Block_GetData(block);
    data = data + BF_BLOCK_SIZE - sizeof(BPLUS_INDEX_NODE);
    memcpy(data,&metadata,sizeof(BPLUS_INDEX_NODE));
}  

/*Creates new index node block with default values and returns its id
  A pointer to the block is also returned through a passed block pointer*/
int NewIndex(int file_desc,BF_Block* block) {
    BPLUS_INDEX_NODE node_metadata;
    int pos;
    BF_AllocateBlock(file_desc,block);

    node_metadata.rec_count = 1;
    set_metadata(block,node_metadata);

    //unpin?
    BF_GetBlockCounter(file_desc,&pos);
    pos--;
    
    return pos;
}

/* Shifts all entries to the right from position pos
    After call, pos is the position now ready for insertion*/
void shift_right(BF_Block* block,int pos) {
    BPLUS_INDEX_NODE node_metadata;
    char *data, *last_rec, *curr, *next;
    int temp_key,temp_block;

    data = BF_Block_GetData(block); //data points to start of index block ie its leftmost block id
    data += sizeof(int); // data now points to first key in index block
    node_metadata = GetIndexMetadata(block);

    if (pos != node_metadata.rec_count) {

        last_rec = data + 2*sizeof(int)*(node_metadata.rec_count - 1); //save last entry's key
        memcpy(&temp_key,last_rec,sizeof(int));

        last_rec = last_rec + sizeof(int);  //save last entry's block id
        memcpy(&temp_block,last_rec,sizeof(int));
    
        // make space for new key-block pair by shifting key-block pairs to the right
        for(int i = node_metadata.rec_count - 1; i > pos; i--) {
            curr = data + i*(2*sizeof(int));    //points to current key-block id pair 
            next = data + 2*(i-1)*(sizeof(int)); //points to key-block id pair at the left of curr
            memcpy(curr,next,2*sizeof(int));
    
        }

        // rewrite saved key-block pair
        data = data + (node_metadata.rec_count)*2*sizeof(int);
        memcpy(data,&temp_key,sizeof(int));
        data = data + sizeof(int);
        memcpy(data,&temp_block,sizeof(int));
    }
}
/*Prints block records*/
void PrintIndex(BF_Block* block) {
    BPLUS_INDEX_NODE metadata;
    char *data;
    int pair[2];
    int last_key;

    metadata = GetIndexMetadata(block);
    data = BF_Block_GetData(block);

    printf("Index Block: ");
    for(int i = 0; i < metadata.rec_count; i++) {
        memcpy(pair,data,2*sizeof(int));
        data += 2*sizeof(int);
        printf("|(%d)|%d|",pair[0],pair[1]);
    }

    if (metadata.rec_count != 0) {
        memcpy(&last_key,data,sizeof(int));
        printf("(%d)|",last_key);
    }

    printf("\n");
}


/* Handles index block splitting 
    Returns key-block_id pair where block_id is the new block 
    from the split and key is the popped middle key of the original block*/
Pair IndexSplit(int file_desc,BF_Block* block) {
    BF_Block *new_block;
    BPLUS_INDEX_NODE metadata;
    Pair pair;
    char *data, *half;
    int split_pos,range,old_sibling,sibling;

    BF_Block_Init(&new_block); 
    
    metadata = GetIndexMetadata(block);

    split_pos = (metadata.rec_count)/2; // position of key to pop
    range = metadata.rec_count - 1 - split_pos; //count of pairs from half to new ovf index block

    data = BF_Block_GetData(block);
    data = data + sizeof(int) + split_pos*(2*sizeof(int)); //points to key to pop
    memcpy(&pair.key,data,sizeof(int)); 

    half = data + sizeof(int); // points to second half of index node

    sibling = NewIndex(file_desc,new_block); //creates new ovf index block, sibling of original block
    data = BF_Block_GetData(new_block);  //points to start of new sibling index block
    memcpy(data,half,sizeof(int) + range*2*sizeof(int)); //copy rightmost half of orignal node to sibling

    metadata.rec_count = split_pos;
    set_metadata(block,metadata);

    metadata.rec_count = range;
    set_metadata(new_block,metadata);

    pair.block = sibling; //set pair block to new ovf block

    //Memory Management
    BF_Block_SetDirty(new_block);
    BF_UnpinBlock(new_block);  
    BF_Block_Destroy(&new_block);
    return pair;
}

Pair InsertIndex(int file_desc,BPLUS_INFO *bplus_info,BF_Block* block,Pair ovf_pair) {
    
    int overflow_block, ovf_key,ovf_block,cur_key;
    Pair new_ovf_pair;
    Record* current_rec;
    BPLUS_INDEX_NODE node_metadata;
    char *data, *cur_data,*metadata;

    //initialize pair struct with default values
    new_ovf_pair.key = -1;
    new_ovf_pair.block = -1;

    data = BF_Block_GetData(block);
    node_metadata = GetIndexMetadata(block);

    ovf_key = ovf_pair.key;    
    ovf_block = ovf_pair.block;

    //iterate over node, find position to insert ovf key-block pair
    int pos = 0;
    while (pos < node_metadata.rec_count ) {
        cur_data = data + sizeof(int) + pos*2*(sizeof(int));
        memcpy(&cur_key,cur_data,sizeof(int));

        if (cur_key > ovf_key) {
            break;
        }
        pos++;
    }  

    shift_right(block,pos); // shift entries to the right
    node_metadata.rec_count++; //increase count of index node keys
    set_metadata(block,node_metadata); //set index node new metadata

    // write ovf key and ovf id to current index block
    data = data + sizeof(int) + pos*2*sizeof(int);
    memcpy(data,&ovf_key,sizeof(int));
    data = data + sizeof(int);
    memcpy(data,&ovf_block,sizeof(int));



    if (bplus_info->index_capacity == node_metadata.rec_count) { //if block reached capacity
        new_ovf_pair = IndexSplit(file_desc,block); //split index node
    }  
    BF_Block_SetDirty(block);
    return new_ovf_pair;
  
}







