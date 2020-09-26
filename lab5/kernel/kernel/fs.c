#include "x86.h"
#include "device.h"
#include "fs.h"
#include "common.h"

int readSuperBlock(SuperBlock *superBlock) {
    diskRead((void*)superBlock, sizeof(SuperBlock), 1, 0);
    return 0;
}

int readBlock (SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    
    if (blockIndex < bound0) {
        diskRead((void*)buffer, sizeof(uint8_t), superBlock->blockSize, inode->pointer[blockIndex] * SECTOR_SIZE);
        return 0;
    }
    else if (blockIndex < bound1) {
        diskRead((void*)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        diskRead((void*)buffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
        return 0;
    }
    else {
        return -1;
    }
}

int writeBlock (SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];

    if (blockIndex < bound0) {
		diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, inode->pointer[blockIndex] * SECTOR_SIZE);
        //fseek(file, inode->pointer[blockIndex] * SECTOR_SIZE, SEEK_SET);
        //fwrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, file);
        return 0;
    }
    else if (blockIndex < bound1) {
		diskRead((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        //fseek(file, inode->singlyPointer * SECTOR_SIZE, SEEK_SET);
        //fread((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, file);
        diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
		//fseek(file, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE, SEEK_SET);
        //fwrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, file);
        return 0;
    }
    else
        return -1;
}

int readInode(SuperBlock *superBlock, Inode *destInode, int *inodeOffset, const char *destFilePath) {
    int i = 0;
    int j = 0;
    int ret = 0;
    int cond = 0;
    *inodeOffset = 0;
    uint8_t buffer[superBlock->blockSize];
    DirEntry *dirEntry = NULL;
    int count = 0;
    int size = 0;
    int blockCount = 0;

    if (destFilePath == NULL || destFilePath[count] == 0) {
        return -1;
    }
    ret = stringChr(destFilePath, '/', &size);
    if (ret == -1 || size != 0) {
        return -1;
    }
    count += 1;
    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE;
    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);

    while (destFilePath[count] != 0) {
        ret = stringChr(destFilePath + count, '/', &size);
        if(ret == 0 && size == 0) {
            return -1;
        }
        if (ret == -1) {
            cond = 1;
        }
        else if (destInode->type == REGULAR_TYPE) {
            return -1;
        }
        blockCount = destInode->blockCount;
        for (i = 0; i < blockCount; i ++) {
            ret = readBlock(superBlock, destInode, i, buffer);
            if (ret == -1) {
                return -1;
            }
            dirEntry = (DirEntry *)buffer;
            for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j ++) {
                if (dirEntry[j].inode == 0) {
                    continue;
                }
                else if (stringCmp(dirEntry[j].name, destFilePath + count, size) == 0) {
                    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE + (dirEntry[j].inode - 1) * sizeof(Inode);
                    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);
                    break;
                }
            }
            if (j < superBlock->blockSize / sizeof(DirEntry)) {
                break;
            }
        }
        if (i < blockCount) {
            if (cond == 0) {
                count += (size + 1);
            }
            else {
                return 0;
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}

// TODO in lab5
int getAvailBlock (SuperBlock *superBlock, int *blockOffset) {
    int j = 0;
    int k = 0;
    int blockBitmapOffset = 0;
    BlockBitmap blockBitmap;

    if (superBlock->availBlockNum == 0)
        return -1;
    superBlock->availBlockNum--;

    blockBitmapOffset = superBlock->blockBitmap;
    diskRead((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    // fseek(file, blockBitmapOffset * SECTOR_SIZE, SEEK_SET);
    // fread((void *)&blockBitmap, sizeof(BlockBitmap), 1, file);
    for (j = 0; j < superBlock->blockNum / 8; j++) {
        if (blockBitmap.byte[j] != 0xff) {
            break;
        }
    }
    for (k = 0; k < 8; k++) {
        if ((blockBitmap.byte[j] >> (7 - k)) % 2 == 0) {
            break;
        }
    }
    blockBitmap.byte[j] = blockBitmap.byte[j] | (1 << (7 - k));
    
    *blockOffset = superBlock->blocks + ((j * 8 + k) * superBlock->blockSize / SECTOR_SIZE);

    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    //fseek(file, 0, SEEK_SET);
    //fwrite((void *)superBlock, sizeof(SuperBlock), 1, file);
    diskWrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    //fseek(file, blockBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fwrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, file);

    return 0;
}

int getAvailInode (SuperBlock *superBlock, int *inodeOffset) {
    int j = 0;
    int k = 0;
    int inodeBitmapOffset = 0;
    int inodeTableOffset = 0;
    InodeBitmap inodeBitmap;

    if (superBlock->availInodeNum == 0)
        return -1;
    superBlock->availInodeNum--;

    inodeBitmapOffset = superBlock->inodeBitmap;
    inodeTableOffset = superBlock->inodeTable;
	diskRead((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    //fseek(file, inodeBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fread((void *)&inodeBitmap, sizeof(InodeBitmap), 1, file);
    for (j = 0; j < superBlock->availInodeNum / 8; j++) {
        if (inodeBitmap.byte[j] != 0xff) {
            break;
        }
    }
    for (k = 0; k < 8; k++) {
        if ((inodeBitmap.byte[j] >> (7-k)) % 2 == 0) {
            break;
        }
    }
    inodeBitmap.byte[j] = inodeBitmap.byte[j] | (1 << (7 - k));

    *inodeOffset = inodeTableOffset * SECTOR_SIZE + (j * 8 + k) * sizeof(Inode);

	diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    //fseek(file, 0, SEEK_SET);
    //fwrite((void *)superBlock, sizeof(SuperBlock), 1, file);
	diskWrite((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    //fseek(file, inodeBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fwrite((void *)&inodeBitmap, sizeof(InodeBitmap), 1, file);

    return 0;
}

int allocInode (SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset,
        Inode *destInode, int *destInodeOffset, const char *destFilename, int destFiletype) {
    int i = 0;
    int j = 0;
    int ret = 0;
    //int blockOffset = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];
    int length = stringLen(destFilename);

    if (destFilename == NULL || destFilename[0] == 0)
        return -1;

    if (superBlock->availInodeNum == 0)
        return -1;
    
    for (i = 0; i < fatherInode->blockCount; i++) {
        ret = readBlock(superBlock, fatherInode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++) {
            if (dirEntry[j].inode == 0) // a valid empty dirEntry
                break;
            else if (stringCmp(dirEntry[j].name, destFilename, length) == 0)
                return -1; // file with filename = destFilename exist
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == fatherInode->blockCount) {
        ret = allocBlock(superBlock, fatherInode, fatherInodeOffset);
        if (ret == -1)
            return -1;
        fatherInode->size = fatherInode->blockCount * superBlock->blockSize;
        setBuffer(buffer, superBlock->blockSize, 0);
        dirEntry = (DirEntry *)buffer;
        j = 0;
    }
    // dirEntry[j] is the valid empty dirEntry, it is in the i-th block of fatherInode.
    ret = getAvailInode(superBlock, destInodeOffset);
    if (ret == -1)
        return -1;

    stringCpy(destFilename, dirEntry[j].name, NAME_LENGTH);
    dirEntry[j].inode = (*destInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    ret = writeBlock(superBlock, fatherInode, i, buffer);
    if (ret == -1)
        return -1;
    
    diskWrite((void *)fatherInode, sizeof(Inode), 1, fatherInodeOffset);
    // fseek(file, fatherInodeOffset, SEEK_SET);
    // fwrite((void *)fatherInode, sizeof(Inode), 1, file);

    destInode->type = destFiletype;
    destInode->linkCount = 1;
    destInode->blockCount = 0;
    destInode->size = 0;

    diskWrite((void *)destInode, sizeof(Inode), 1, *destInodeOffset);
    // fseek(file, *destInodeOffset, SEEK_SET);
    // fwrite((void *)destInode, sizeof(Inode), 1, file);
    return 0;
}

int calNeededPointerBlocks (SuperBlock *superBlock, int blockCount) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    if (blockCount == bound0)
        return 1;
    else if (blockCount >= bound1)
        return -1;
    else
        return 0;
}

int allocLastBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset, int blockOffset) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    int singlyPointerBufferOffset = 0;

    if (inode->blockCount < bound0) {
        inode->pointer[inode->blockCount] = blockOffset;
    }
    else if (inode->blockCount == bound0) {
        getAvailBlock(superBlock, &singlyPointerBufferOffset);
        singlyPointerBuffer[0] = blockOffset;
        inode->singlyPointer = singlyPointerBufferOffset;
        diskWrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBufferOffset * SECTOR_SIZE);
        //fseek(file, singlyPointerBufferOffset * SECTOR_SIZE, SEEK_SET);
        //fwrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, file);
    }
    else if (inode->blockCount < bound1) {
        diskRead((void*)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        //fseek(file, inode->singlyPointer * SECTOR_SIZE, SEEK_SET);
        //fread((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, file);
        singlyPointerBuffer[inode->blockCount - bound0] = blockOffset;
        diskWrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        //fseek(file, inode->singlyPointer * SECTOR_SIZE, SEEK_SET);
        //fwrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, file);
    }
    else
        return -1;
    
    inode->blockCount++;
    diskWrite((void *)inode, sizeof(Inode), 1, inodeOffset);
    //fseek(file, inodeOffset, SEEK_SET);
    //fwrite((void *)inode, sizeof(Inode), 1, file);
    return 0;
}

int allocBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset) {
    int ret = 0;
    int blockOffset = 0;

    ret = calNeededPointerBlocks(superBlock, inode->blockCount);
    if (ret == -1)
        return -1;
    if (superBlock->availBlockNum < ret + 1)
        return -1;
    
    getAvailBlock(superBlock, &blockOffset);
    allocLastBlock(superBlock, inode, inodeOffset, blockOffset);
    return 0;
}

int createFile(SuperBlock *superBlock, const char *destFilePath, Inode *destInode, int *destInodeOffset){
    char tmp = 0;
    int ret = 0;
    int size = 0;
    int fatherInodeOffset = 0;
    Inode fatherInode;
	
    if (destFilePath == NULL)
        return -1;
    ret = stringChrR(destFilePath, '/', &size);
    if (ret == -1)
        return -1;
    tmp = *((char*)destFilePath + size + 1);
    *((char*)destFilePath + size + 1) = 0;
    ret = readInode(superBlock, &fatherInode, &fatherInodeOffset, destFilePath);
    *((char*)destFilePath + size + 1) = tmp;
    if (ret == -1)
        return -1;
    ret = allocInode(superBlock, &fatherInode, fatherInodeOffset,
        destInode, destInodeOffset, destFilePath + size + 1, REGULAR_TYPE);
    if (ret == -1)
        return -1;
    
    return 0;
}

int initDir(SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset,
        Inode *destInode, int destInodeOffset) {
    int ret = 0;
    int blockOffset = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    ret = getAvailBlock(superBlock, &blockOffset);
    if (ret == -1)
        return -1;
    destInode->pointer[0] = blockOffset;
    destInode->blockCount = 1;
    destInode->size = superBlock->blockSize;
    setBuffer(buffer, superBlock->blockSize, 0);
    dirEntry = (DirEntry *)buffer;
    dirEntry[0].inode = (destInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    destInode->linkCount ++;
    dirEntry[0].name[0] = '.';
    dirEntry[0].name[1] = '\0';
    dirEntry[1].inode = (fatherInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    fatherInode->linkCount ++;
    dirEntry[1].name[0] = '.';
    dirEntry[1].name[1] = '.';
    dirEntry[1].name[2] = '\0';

    diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, blockOffset * SECTOR_SIZE);
    //fseek(file, blockOffset * SECTOR_SIZE, SEEK_SET);
    //fwrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, file);
    diskWrite((void *)fatherInode, sizeof(Inode), 1, fatherInodeOffset);
    //fseek(file, fatherInodeOffset, SEEK_SET);
    //fwrite((void *)fatherInode, sizeof(Inode), 1, file);
    diskWrite((void *)destInode, sizeof(Inode), 1, destInodeOffset);
    //fseek(file, destInodeOffset, SEEK_SET);
    //fwrite((void *)destInode, sizeof(Inode), 1, file);
    return 0;
}

int getDirEntry (SuperBlock *superBlock, Inode *inode, int dirIndex, DirEntry *destDirEntry) {
    int i = 0;
    int j = 0;
    int ret = 0;
    int dirCount = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    for (i = 0; i < inode->blockCount; i++) {
        ret = readBlock(superBlock, inode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j ++) {
            if (dirEntry[j].inode != 0) {
                if (dirCount == dirIndex)
                    break;
                else
                    dirCount ++;
            }
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == inode->blockCount)
        return -1;
    else {
        destDirEntry->inode = dirEntry[j].inode;
        stringCpy(dirEntry[j].name, destDirEntry->name, NAME_LENGTH);
        return 0;
    }
}

int mkdir (SuperBlock *superBlock, const char *destDirPath, Inode *destInode, int *destInodeOffset) {
    char tmp = 0;
    int length = 0;
    int cond = 0;
    int ret = 0;
    int size = 0;
    int fatherInodeOffset = 0;
    Inode fatherInode;


    if (destDirPath == NULL)
        return -1;
    length = stringLen(destDirPath);
    if (destDirPath[length - 1] == '/') {
        cond = 1;
        *((char*)destDirPath + length - 1) = 0;
    }
    ret = stringChrR(destDirPath, '/', &size);
    if (ret == -1)
        return -1;
    tmp = *((char*)destDirPath + size + 1);
    *((char*)destDirPath + size + 1) = 0;
    ret = readInode(superBlock, &fatherInode, &fatherInodeOffset, destDirPath);
    *((char*)destDirPath + size + 1) = tmp;
    if (ret == -1) {
        if (cond == 1)
            *((char*)destDirPath + length - 1) = '/';
        return -1;
    }
    ret = allocInode(superBlock, &fatherInode, fatherInodeOffset,
        destInode, destInodeOffset, destDirPath + size + 1, DIRECTORY_TYPE);
    if (ret == -1) {
        if (cond == 1)
            *((char*)destDirPath + length - 1) = '/';
        return -1;
    }
    ret = initDir(superBlock, &fatherInode, fatherInodeOffset,
        destInode, *destInodeOffset);
    if (ret == -1) {
        if (cond == 1)
            *((char*)destDirPath + length - 1) = '/';
        return -1;
    }
    if (cond == 1)
        *((char*)destDirPath + length - 1) = '/';
    
    return 0;
}

int clrBlocks(SuperBlock *superBlock, Inode *inode) {
    // TODO in lab5
    int i = 0;
    int j = 0;
    int k = 0;
    int ret = 0;
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;
    uint32_t singlyPointerBuffer[divider0];
    uint8_t buffer[superBlock->blockSize];

    BlockBitmap blockBitmap;
    int blockBitmapOffset = superBlock->blockBitmap;
    diskRead((void*)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    //fseek(file, blockBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fread((void *)&blockBitmap, sizeof(BlockBitmap), 1, file);

    setBuffer(buffer, superBlock->blockSize, 0);

    for (i = 0; i < inode->blockCount; i++) {
        ret = writeBlock(superBlock, inode, i, buffer);
        if (ret == -1)
            return -1;
        if (i < bound0) {
            j = inode->pointer[i] / SECTORS_PER_BLOCK / 8;
            k = inode->pointer[i] / SECTORS_PER_BLOCK - j * 8;
            blockBitmap.byte[j] = blockBitmap.byte[j] & ~(1 << (7 - k));
            superBlock->availBlockNum++;
        }
        else if (i < bound1) {
            diskRead((void*)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
            //fseek(file, inode->singlyPointer * SECTOR_SIZE, SEEK_SET);
            //fread((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, file);
            j = singlyPointerBuffer[i - bound0] / SECTORS_PER_BLOCK / 8;
            k = singlyPointerBuffer[i - bound0] / SECTORS_PER_BLOCK - j * 8;
            blockBitmap.byte[j] = blockBitmap.byte[j] & ~(1 << (7 - k));
            superBlock->availBlockNum++;
        }
        else
            return -1;
    }

    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    //fseek(file, 0, SEEK_SET);
    //fwrite((void *)superBlock, sizeof(SuperBlock), 1, file);
    diskWrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    //fseek(file, blockBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fwrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, file);
    return 0;
}

int clrInode(SuperBlock *superBlock, int inodeOffset) {
    // TODO in lab5
    int j = 0;
    int k = 0;
    InodeBitmap inodeBitmap;
    int inodeBitmapOffset = superBlock->inodeBitmap;
	
    diskRead((void*)&inodeBitmap, sizeof(BlockBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    //fseek(file, inodeBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fread((void *)&inodeBitmap, sizeof(BlockBitmap), 1, file);

    j = (inodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) / 8;
    k = (inodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) - j * 8;
    inodeBitmap.byte[j] = inodeBitmap.byte[j] & ~(1 << (7 - k));
    superBlock->availInodeNum++;

    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    //fseek(file, 0, SEEK_SET);
    //fwrite((void *)superBlock, sizeof(SuperBlock), 1, file);
    diskWrite((void *)&inodeBitmap, sizeof(BlockBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    //fseek(file, inodeBitmapOffset * SECTOR_SIZE, SEEK_SET);
    //fwrite((void *)&inodeBitmap, sizeof(BlockBitmap), 1, file);
    return 0;
}

int delDirEntry (SuperBlock *superBlock, Inode *fatherInode, int destInodeOffset) {
    // TODO in lab5
    int i = 0;
    int j = 0;
    int ret = 0;
    DirEntry *dirEntry;
    int childInodeOffset = 0;
    uint8_t buffer[superBlock->blockSize];

    for (i = 0; i < fatherInode->blockCount; i++) {
        ret = readBlock(superBlock, fatherInode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j ++) {
            if (dirEntry[j].inode != 0) {
                childInodeOffset = superBlock->inodeTable * SECTOR_SIZE + (dirEntry[j].inode - 1) * sizeof(Inode);
                if(childInodeOffset == destInodeOffset) {
                    dirEntry[j].inode = 0;
                    dirEntry[j].name[0] = 0;
                    break;
                }
            }
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if(i == fatherInode->blockCount)
        return -1;
    ret = writeBlock(superBlock, fatherInode, i, buffer);
    return ret;
}

int clrDirEntries (SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset, 
                Inode *destInode, int destInodeOffset) {
    // TODO in lab5
    int ret = 0;
    int dirIndex = 0;
    DirEntry dirEntry;
    Inode childInode;
    int childInodeOffset = 0;

    while (getDirEntry(superBlock, destInode, dirIndex, &dirEntry) == 0) {
        dirIndex++;
        childInodeOffset = superBlock->inodeTable * SECTOR_SIZE + (dirEntry.inode - 1) * sizeof(Inode);
        diskRead((void*)&childInode, sizeof(Inode), 1, childInodeOffset);
        //fseek(file, childInodeOffset, SEEK_SET);
        //fread((void*)&childInode, sizeof(Inode), 1, file);
        if(childInode.type == DIRECTORY_TYPE) {
            if(childInodeOffset == fatherInodeOffset || childInodeOffset == destInodeOffset) {
                childInode.linkCount--;
                diskWrite((void *)&childInode, sizeof(Inode), 1, childInodeOffset);
                //fseek(file, childInodeOffset, SEEK_SET);
                //fwrite((void *)&childInode, sizeof(Inode), 1, file);
            }
            else {
                ret = clrDirEntries(superBlock, destInode, destInodeOffset, &childInode, childInodeOffset);
                if(ret == -1)
                    return -1;
                ret = clrInode(superBlock, childInodeOffset);
            }
        }
        else {
            ret = clrBlocks(superBlock, &childInode);
            if(ret == -1)
                return -1;
            ret = clrInode(superBlock, childInodeOffset);
        }
    }
    ret = clrBlocks(superBlock, destInode);
    if(ret == -1)
        return -1;

    return 0;
}

int rm (const char *destFilePath) {
    // TODO
    char tmp = 0;
    int ret = 0;
    int size = 0;
    int length = 0;
    SuperBlock superBlock;
    int fatherInodeOffset = 0;  // byte as unit
    Inode fatherInode;          // Inode for dir
    int destInodeOffset = 0;    // byte as unit
    Inode destInode;            // Inode for name

    if (destFilePath == NULL)
        return -1;
    ret = readSuperBlock(&superBlock);
    if (ret == -1)
        return -1;
    length = stringLen(destFilePath);
    if (destFilePath[0] != '/' || destFilePath[length - 1] == '/')
        return -1;
    ret = stringChrR(destFilePath, '/', &size);
    if (ret == -1)  // no '/' in destFilePath
        return -1;
    tmp = *((char*)destFilePath + size + 1);
    *((char*)destFilePath + size + 1) = 0;  // destFilePath is dir ended with '/'.
    ret = readInode(&superBlock, &fatherInode, &fatherInodeOffset, destFilePath);
    *((char*)destFilePath + size + 1) = tmp;
    if (ret == -1)
        return -1;
    if(fatherInode.type != DIRECTORY_TYPE)
        return -1;
    ret = readInode(&superBlock, &destInode, &destInodeOffset, destFilePath);
    if (ret == -1)
        return -1;
    if(destInode.type != REGULAR_TYPE)
        return -1;

    ret = clrBlocks(&superBlock, &destInode);
    if(ret == -1)
        return -1;
    ret = clrInode(&superBlock, destInodeOffset);

    ret = delDirEntry(&superBlock, &fatherInode, destInodeOffset);
    if(ret == -1)
        return -1;

    return 0;
}

int rmdir (const char *destDirPath) {
    // TODO
    int ret = 0;
    int size = 0;
    int length = 0;
    int cond = 0;
    char tmp = 0;
    SuperBlock superBlock;
    int destInodeOffset = 0;    // byte as unit
    Inode destInode;            // Inode for name
    int fatherInodeOffset = 0;
    Inode fatherInode;

    if (destDirPath == NULL)
        return -1;
    ret = readSuperBlock(&superBlock);
    if (ret == -1)
        return -1;
    length = stringLen(destDirPath);
    if (destDirPath[length - 1] == '/') {
        cond = 1;
        *((char*)destDirPath + length - 1) = 0;
    }
    ret = stringChrR(destDirPath, '/', &size);
    if (ret == -1)
        return -1;
    tmp = *((char*)destDirPath + size + 1);
    *((char*)destDirPath + size + 1) = 0;
    ret = readInode(&superBlock, &fatherInode, &fatherInodeOffset, destDirPath);
    *((char*)destDirPath + size + 1) = tmp;
    if (ret == -1) {
        if (cond == 1)
            *((char*)destDirPath + length - 1) = '/';
        return -1;
    }
    ret = readInode(&superBlock, &destInode, &destInodeOffset, destDirPath);
    if (ret == -1)
        return -1;
    if(destInode.type != DIRECTORY_TYPE)
        return -1;

    ret = clrDirEntries(&superBlock, &fatherInode, fatherInodeOffset, &destInode, destInodeOffset);
    if(ret == -1)
        return -1;
    ret = clrInode(&superBlock, destInodeOffset);
    if (cond == 1)
        *((char*)destDirPath + length - 1) = '/';
    ret = delDirEntry(&superBlock, &fatherInode, destInodeOffset);
    if(ret == -1)
        return -1;
    
    return 0;
}
