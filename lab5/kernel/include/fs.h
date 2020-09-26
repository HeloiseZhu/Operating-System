#ifndef __FS_H__
#define __FS_H__

#include "fs/minix.h"

int readSuperBlock (SuperBlock *superBlock);

int allocInode (SuperBlock *superBlock,
                Inode *fatherInode,
                int fatherInodeOffset,
                Inode *destInode,
                int *destInodeOffset,
                const char *destFilename,
                int destFiletype);

int freeInode (SuperBlock *superBlock,
               Inode *fatherInode,
               int fatherInodeOffset,
               Inode *destInode,
               int *destInodeOffset,
               const char *destFilename,
               int destFiletype);

int readInode (SuperBlock *superBlock,
               Inode *destInode,
               int *inodeOffset,
               const char *destFilePath);

int allocBlock (SuperBlock *superBlock,
                Inode *inode,
                int inodeOffset);

int readBlock (SuperBlock *superBlock,
               Inode *inode,
               int blockIndex,
               uint8_t *buffer);

int writeBlock (SuperBlock *superBlock,
                Inode *inode,
                int blockIndex,
                uint8_t *buffer);

int getDirEntry (SuperBlock *superBlock,
                 Inode *inode,
                 int dirIndex,
                 DirEntry *destDirEntry);

// TODO in lab5
int calNeededPointerBlocks(SuperBlock *superBlock,
                           int blockCount);

int getAvailBlock(SuperBlock *superBlock,
                  int *blockOffset);

int allocLastBlock(SuperBlock *superBlock,
                   Inode *inode,
                   int inodeOffset,
                   int blockOffset);

int getAvailInode(SuperBlock *superBlock, 
				  int *inodeOffset);

int getDirEntry(SuperBlock *superBlock,
                Inode *inode,
                int dirIndex,
                DirEntry *destDirEntry);

int initDir(SuperBlock *superBlock,
            Inode *fatherInode,
            int fatherInodeOffset,
            Inode *destInode,
            int destInodeOffset);

int clrBlocks(SuperBlock *superBlock,
              Inode *inode);

int clrInode(SuperBlock *superBlock,
             int inodeOffset);

int delDirEntry(SuperBlock *superBlock,
                Inode *fatherInode,
                int destInodeOffset);

int clrDirEntries(SuperBlock *superBlock,
                  Inode *fatherInode,
                  int fatherInodeOffset,
                  Inode *destInode,
                  int destInodeOffset);

int createFile(SuperBlock *superBlock,
               const char *destFilePath,
               Inode *destInode,
               int *destInodeOffset);

int mkdir(SuperBlock *superBlock,
          const char *destDirPath,
          Inode *destInode,
          int *destInodeOffset);

int rm(const char *destFilePath);

int rmdir(const char *destDirPath);



void initFS (void);

void initFile (void);

#endif /* __FS_H__ */
