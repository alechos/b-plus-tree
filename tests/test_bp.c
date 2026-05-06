#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bf.h"
#include "bp_file.h"
#include "bp_datanode.h"
#include "bp_indexnode.h"

#define FILE_NAME "data.db"
#define RECORDS_NUM 1000

Record test_records[RECORDS_NUM];
static int records_initialized = 0;  

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

void testCreateFile();
void testOpenFile();
void testCloseFile();
void testInsertEntry();
void testGetEntry();
void testGetMissing();

int main() {
    printf("Testing B+ Tree functions\n");
    printf("Testing CreateFile\n");
    testCreateFile();
    printf("Testing OpenFile\n");
    testOpenFile();
    printf("Testing CloseFile\n");
    testCloseFile();
    printf("Testing InsertEntry\n");
    testInsertEntry();
    printf("Testing GetEntry\n");
    testGetEntry();
    printf("Testing GetEntry (missing key)\n");
    testGetMissing();
    return 0;
}

void testCreateFile() {
    BF_Init(LRU);
    if (access(FILE_NAME, F_OK) == 0) {
        printf("File already present. Aborting...\n");
        BF_Close();
        return;
    }
    int result = BP_CreateFile(FILE_NAME);
    printf("BP_CreateFile: %s\n", result == 0 ? "SUCCESS" : "FAIL");
    BF_Close();
}

void testOpenFile() {
    int desc;
    BPLUS_INFO *info;
    BF_Init(LRU);
    info = BP_OpenFile(FILE_NAME, &desc);
    if (info != NULL) {
        printf("BP_OpenFile: SUCCESS\n");
        // Fix: close file before freeing info
        BP_CloseFile(desc, info);
    } else {
        printf("BP_OpenFile: FAIL\n");
    }
    BF_Close();
}

void testCloseFile() {
    int desc;
    BPLUS_INFO *info;
    BF_Init(LRU);
    info = BP_OpenFile(FILE_NAME, &desc);
    if (info != NULL) {
        int result = BP_CloseFile(desc, info);
        printf("BP_CloseFile: %s\n", result == 0 ? "SUCCESS" : "FAIL");
    } else {
        printf("Corrupted Metadata, aborting...\n");
        printf("BP_CloseFile: FAIL\n");
    }
    BF_Close();
}

void testInsertEntry() {
    int desc, success = 1;
    Record tempRec, *result;
    BPLUS_INFO *info;
    result = &tempRec;

    BF_Init(LRU);
    info = BP_OpenFile(FILE_NAME, &desc);
    if (info == NULL) {
        printf("Corrupted Metadata, aborting...\n");
        printf("BP_InsertEntry: FAIL\n");  // Fix: was /n
        BF_Close();
        return;
    }

    for (int i = 0; i < RECORDS_NUM; i++) {
        test_records[i] = randomRecord();
        BP_InsertEntry(desc, info, test_records[i]);
    }
    records_initialized = 1;  // mark records as valid

    for (int i = 0; i < RECORDS_NUM; i++) {
        if (BP_GetEntry(desc, info, test_records[i].id, &result) == -1) {
            success = 0;
            break;
        }
    }
    printf("BP_InsertEntry: %s\n", success ? "SUCCESS" : "FAIL");

    BP_CloseFile(desc, info);
    BF_Close();
}

void testGetEntry() {

    if (!records_initialized) {
        printf("BP_GetEntry: SKIPPED (InsertEntry did not complete)\n");
        return;
    }

    int desc, success = 1;
    Record tempRec;
    BPLUS_INFO *info;
    Record *result = &tempRec;

    BF_Init(LRU);
    info = BP_OpenFile(FILE_NAME, &desc);
    if (info == NULL) {
        printf("Corrupted Metadata, aborting...\n");
        printf("BP_GetEntry: FAIL\n"); 
        BF_Close();
        return;
    }

    for (int i = 0; i < RECORDS_NUM; i++) {
        if (BP_GetEntry(desc, info, test_records[i].id, &result) == -1) {
            success = 0;
            break;
        }
    }
    printf("BP_GetEntry: %s\n", success ? "SUCCESS" : "FAIL");

    BP_CloseFile(desc, info);
    BF_Close();
}

void testGetMissing() {
    int desc;
    Record tempRec;
    BPLUS_INFO *info;
    Record *result = &tempRec;

    BF_Init(LRU);
    info = BP_OpenFile(FILE_NAME, &desc);
    if (info == NULL) {
        printf("Corrupted Metadata, aborting...\n");
        printf("BP_GetMissing: FAIL\n");
        BF_Close();
        return;
    }

    int missing_key = -1;
    int ret = BP_GetEntry(desc, info, missing_key, &result);
    printf("BP_GetEntry (missing key): %s\n", ret == -1 ? "SUCCESS" : "FAIL");

    BP_CloseFile(desc, info);
    BF_Close();
}
