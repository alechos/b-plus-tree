#ifndef BF_H
#define BF_H

#ifdef __cplusplus
extern "C" {
#endif

#define BF_BLOCK_SIZE 512      /* Size of a block in bytes */
#define BF_BUFFER_SIZE 100     /* Maximum number of blocks kept in memory */
#define BF_MAX_OPEN_FILES 100  /* Maximum number of open files */

typedef enum BF_ErrorCode {
  BF_OK,
  BF_OPEN_FILES_LIMIT_ERROR,     /* BF_MAX_OPEN_FILES files are already open */
  BF_INVALID_FILE_ERROR,         /* File descriptor does not correspond to any open file */
  BF_ACTIVE_ERROR,               /* BF layer is active and cannot be re-initialized */
  BF_FILE_ALREADY_EXISTS,        /* File cannot be created because it already exists */
  BF_FULL_MEMORY_ERROR,          /* Memory is full of active blocks */
  BF_INVALID_BLOCK_NUMBER_ERROR, /* Requested block does not exist in the file */
  BF_AVAILABLE_PIN_BLOCKS_ERROR, /* File cannot be closed because there are pinned blocks in memory */
  BF_ERROR
} BF_ErrorCode;

typedef enum ReplacementAlgorithm {
  LRU,
  MRU
} ReplacementAlgorithm;


// Block structure
typedef struct BF_Block BF_Block;

/*
 * BF_Block_Init initializes and allocates the appropriate memory
 * for the BF_Block structure.
 */
void BF_Block_Init(BF_Block **block);

/*
 * BF_Block_Destroy frees the memory occupied by the BF_Block structure.
 */
void BF_Block_Destroy(BF_Block **block);

/*
 * BF_Block_SetDirty marks the block as dirty.
 * This means the block's data has been modified and the BF layer
 * will write it back to disk when needed. If you are only reading
 * data without modifying it, this function does not need to be called.
 */
void BF_Block_SetDirty(BF_Block *block);

/*
 * BF_Block_GetData returns a pointer to the block's data.
 * If the data is modified, the block must be marked dirty by calling
 * BF_Block_SetDirty.
 */
char* BF_Block_GetData(const BF_Block *block);

/*
 * BF_Init initializes the BF layer. You can choose between two block
 * replacement policies: LRU and MRU.
 */
BF_ErrorCode BF_Init(const ReplacementAlgorithm repl_alg);

/*
 * BF_CreateFile creates a block-based file with the given filename.
 * Returns an error code if the file already exists.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_CreateFile(const char* filename);

/*
 * BF_OpenFile opens an existing block-based file with the given filename
 * and returns its file descriptor in file_desc.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_OpenFile(const char* filename, int *file_desc);

/*
 * BF_CloseFile closes the open file identified by file_desc.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_CloseFile(const int file_desc);

/*
 * BF_GetBlockCounter returns the number of available blocks in the open
 * file identified by file_desc, via the blocks_num output parameter.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_GetBlockCounter(const int file_desc, int *blocks_num);

/*
 * BF_AllocateBlock allocates a new block for the file identified by file_desc.
 * The new block is always appended at the end of the file, so its number is
 * BF_GetBlockCounter(file_desc) - 1. The allocated block is pinned in memory
 * and returned via the block parameter. Call BF_UnpinBlock when done with it.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_AllocateBlock(const int file_desc, BF_Block *block);

/*
 * BF_GetBlock retrieves the block numbered block_num from the open file
 * identified by file_desc, returning it via the block parameter.
 * The block is pinned in memory. Call BF_UnpinBlock when done with it.
 * On success returns BF_OK; on failure returns an error code.
 * Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_GetBlock(const int file_desc,
                         const int block_num,
                         BF_Block *block);

/*
 * BF_UnpinBlock unpins the block, allowing the BF layer to write it to
 * disk at some point. On success returns BF_OK; on failure returns an
 * error code. Call BF_PrintError to get a description of the error.
 */
BF_ErrorCode BF_UnpinBlock(BF_Block *block);

/*
 * BF_PrintError prints a description of the given error code to stderr.
 */
void BF_PrintError(BF_ErrorCode err);

/*
 * BF_Close shuts down the BF layer, writing any in-memory blocks to disk.
 */
BF_ErrorCode BF_Close();

#ifdef __cplusplus
}
#endif
#endif // BF_H
