#include "x86.h"
#include "device.h"
#include "fs.h"

#define SYS_WRITE 0
#define SYS_FORK 1
#define SYS_EXEC 2
#define SYS_SLEEP 3
#define SYS_EXIT 4
#define SYS_READ 5
#define SYS_SEM 6
#define SYS_GETPID 7
#define SYS_OPEN 8
#define SYS_LSEEK 9
#define SYS_CLOSE 10
#define SYS_REMOVE 11
#define SYS_CREATE 12

#define STD_OUT 0
#define STD_IN 1
#define SH_MEM 3

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

#define O_WRITE 0x01
#define O_READ 0x02
#define O_CREATE 0x04
#define O_DIRECTORY 0x08

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];
extern SysFile sysFcb[MAX_SYSFCB_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

uint8_t shMem[MAX_SHMEM_SIZE];

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);
void syscallSem(struct TrapFrame *tf);
void syscallGetPid(struct TrapFrame *tf);

void syscallOpen(struct TrapFrame *tf);
void syscallLseek(struct TrapFrame *tf);
void syscallClose(struct TrapFrame *tf);
void syscallRemove(struct TrapFrame *tf);
void syscallCreate(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void syscallWriteStdOut(struct TrapFrame *tf);
void syscallReadStdIn(struct TrapFrame *tf);
void syscallWriteShMem(struct TrapFrame *tf);
void syscallReadShMem(struct TrapFrame *tf);
void syscallReadFile(struct TrapFrame *tf);
void syscallWriteFile(struct TrapFrame *tf);
void syscallReadDir(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

void syscallSemInit(struct TrapFrame *tf);
void syscallSemWait(struct TrapFrame *tf);
void syscallSemPost(struct TrapFrame *tf);
void syscallSemDestroy(struct TrapFrame *tf);

void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));

	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)tf;

	switch(tf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(tf); // return
			break;
		case 0x20:
			timerHandle(tf);         // return or iret
			break;
		case 0x21:
			keyboardHandle(tf);      // return
			break;
		case 0x80:
			syscallHandle(tf);       // return
			break;
		default:assert(0);
	}

	pcb[current].stackTop = tmpStackTop;
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case SYS_WRITE:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(tf);
			break; // for SYS_READ
		case SYS_FORK:
			syscallFork(tf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(tf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(tf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(tf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(tf);
			break; // for SYS_SEM
		case SYS_GETPID:
			syscallGetPid(tf);
			break; // for SYS_GETPID
		case SYS_OPEN:
			syscallOpen(tf);
			break; // for SYS_OPEN
		case SYS_LSEEK:
			syscallLseek(tf);
			break; // for SYS_SEEK
		case SYS_CLOSE:
			syscallClose(tf);
			break; // for SYS_CLOSE
		case SYS_REMOVE:
			syscallRemove(tf);
			break; // for SYS_REMOVE
		case SYS_CREATE:
			syscallCreate(tf);
			break; // for SYS_CREATE
		default:
			break;
	}
}

void timerHandle(struct TrapFrame *tf) {
	uint32_t tmpStackTop;
	int i = (current + 1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED) {
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0) {
				pcb[i].state = STATE_RUNNABLE;
			}
		}
		i = (i + 1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
			pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		i = (current + 1) % MAX_PCB_NUM;
		while (i != current) {
			if (i != 0 && pcb[i].state == STATE_RUNNABLE) {
				break;
			}
			i = (i + 1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE) {
			i = 0;
		}
		current = i;
		// putChar('0' + current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop);
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
	return;
}

void keyboardHandle(struct TrapFrame *tf) {
	// TODO in lab4
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if(keyCode != 0) {
		keyBuffer[bufferTail] = keyCode;
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
		if(dev[STD_IN].value < 0){
			dev[STD_IN].value++;
			// get a blocked process
			pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev) -
				(uint32_t)&(((ProcessTable*)0)->blocked));
			dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
			(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
			// wake up the process
			pt->state = STATE_RUNNABLE;
		}
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1) {
				syscallWriteStdOut(tf);
			}
			break; // for STD_OUT
		case STD_IN:
			pcb[current].regs.eax = 0;
			break;
		case SH_MEM:
			if (dev[SH_MEM].state == 1) {
				syscallWriteShMem(tf);
			}
			break; // for SH_MEM
		default:
			if(pcb[current].fcb[tf->ecx].state == 1 && (pcb[current].fcb[tf->ecx].flags & O_WRITE) == O_WRITE) {
				if((pcb[current].fcb[tf->ecx].flags & O_DIRECTORY) == O_DIRECTORY) {
					pcb[current].regs.eax = 0;
				}
				else {
					syscallWriteFile(tf);
				}
			}
			else {
				pcb[current].regs.eax = 0;
			}
			break;
	}
}

void syscallWriteStdOut(struct TrapFrame *tf) {
	int sel = tf->ds; //TODO segment selector for user data, need further modification
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
		if (character == '\n') {
			displayRow++;
			displayCol = 0;
			if (displayRow == 25){
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80){
				displayRow++;
				displayCol = 0;
				if (displayRow == 25){
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	//TODO take care of return value
}

void syscallWriteShMem(struct TrapFrame *tf) {
	// TODO in lab4
	int sel = tf->ds;
	uint8_t *buf = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int index = tf->esi;
	int i = 0;
	uint8_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(data):"r"(buf + i));
		if(index + i < MAX_SHMEM_SIZE) {
			shMem[index + i] = data;
		}
		else
			break;
	}
	pcb[current].regs.eax = i;
}

void syscallWriteFile(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int ret = 0;
	int sel = tf->ds;
	int fd = tf->ecx;
	int size = tf->ebx;
	char character = 0;
	uint8_t *destBuffer = (uint8_t *)tf->edx;
	uint8_t buffer[1024];
	SuperBlock superBlock;
	Inode inode;
	int inodeOffset = 0;
	int offset = 0;
	int blockIndex = 0;
	int base = 0;

	asm volatile("movw %0, %%es" ::"m"(sel));
	pcb[current].regs.eax = 0;
	
	ret = readSuperBlock(&superBlock);
	if(ret == -1)
		return;

	inodeOffset = sysFcb[pcb[current].fcb[fd].index].inodeOffset;
	offset = sysFcb[pcb[current].fcb[fd].index].offset;
	diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);

	if(offset > inode.size)
		offset = inode.size;
	base = offset % superBlock.blockSize;
	blockIndex = offset / superBlock.blockSize;
	if(blockIndex == inode.blockCount) {
		ret = allocBlock(&superBlock, &inode, inodeOffset);
		if(ret == -1)
			return;
		inode.blockCount++;
	}
	ret = readBlock(&superBlock, &inode, blockIndex, buffer);
	if(ret == -1)
		return;
	
	
	while(i < size) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(destBuffer + i));
		buffer[(base + i) % superBlock.blockSize] = character;
		i++;
		if((base + i) % superBlock.blockSize == 0) {
			base = 0;
			ret = writeBlock(&superBlock, &inode, blockIndex, buffer);
			blockIndex++;
			if(ret == -1) {
				if(sysFcb[pcb[current].fcb[fd].index].offset > inode.size) {
					diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);
					inode.size = sysFcb[pcb[current].fcb[fd].index].offset;
					diskWrite((void *)&inode, sizeof(Inode), 1, inodeOffset);
				}
				return;
			}
			sysFcb[pcb[current].fcb[fd].index].offset += (superBlock.blockSize - base);
			pcb[current].regs.eax += (superBlock.blockSize - base);
			if(i < size && blockIndex == inode.blockCount) {
				ret = allocBlock(&superBlock, &inode, inodeOffset);
				if(ret == -1)
					return;
				inode.blockCount++;
			}
		}
	}
	if((base + i) % superBlock.blockSize != 0) {
		ret = writeBlock(&superBlock, &inode, blockIndex, buffer);
		if(ret == -1)
			return;
	}
	
	sysFcb[pcb[current].fcb[fd].index].offset = offset + i;
	pcb[current].regs.eax = i;
	if(sysFcb[pcb[current].fcb[fd].index].offset > inode.size) {
		diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);
		inode.size = sysFcb[pcb[current].fcb[fd].index].offset;
		diskWrite((void *)&inode, sizeof(Inode), 1, inodeOffset);
	}
}

void syscallRead(struct TrapFrame *tf) {
	switch(tf->ecx) {
		case STD_OUT:
			pcb[current].regs.eax = 0;
			break;
		case STD_IN:
			if (dev[STD_IN].state == 1) {
				syscallReadStdIn(tf);
			}
			break;
		case SH_MEM:
			if (dev[SH_MEM].state == 1) {
				syscallReadShMem(tf);
			}
			break;
		default:
			if(pcb[current].fcb[tf->ecx].state == 1 && (pcb[current].fcb[tf->ecx].flags & O_READ) == O_READ) {
				if((pcb[current].fcb[tf->ecx].flags & O_DIRECTORY) == O_DIRECTORY) {
					syscallReadDir(tf);
				}
				else {
					syscallReadFile(tf);
				}
			}
			else {
				pcb[current].regs.eax = 0;
			}
			break;
	}
}

void syscallReadStdIn(struct TrapFrame *tf) {
	// TODO in lab4
	if(dev[STD_IN].value < 0) {
		// set return value
		pcb[current].regs.eax = -1;
	}
	else if(dev[STD_IN].value == 0){
		dev[STD_IN].value--;
		// block current process
		pcb[current].blocked.next = dev[STD_IN].pcb.next;
		pcb[current].blocked.prev = &(dev[STD_IN].pcb);
		dev[STD_IN].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		pcb[current].state = STATE_BLOCKED;
		//pcb[current].sleepTime = 0;
		asm volatile("int $0x20");
		// wake up
		int sel = tf->ds;
		char *str = (char *)tf->edx;
		int i = 0;
		char character;
		int size = (int)tf->ebx;
		asm volatile("movw %0, %%es" ::"m"(sel));
		while(i < size - 1) {
			if(bufferHead == bufferTail)
				break;
			character = getChar(keyBuffer[bufferHead]);
			bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
			if(character != 0){
				asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(str + i));
				i++;
			}
		}
		character = '\0';
		asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(str + i));
		// set return value
		pcb[current].regs.eax = i;
	}
}

void syscallReadShMem(struct TrapFrame *tf) {
	// TODO in lab4
	int sel = tf->ds;
	uint8_t *buf = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int index = tf->esi;
	int i = 0;
	uint8_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++) {
		if(index + i < MAX_SHMEM_SIZE) {
			data = shMem[index + i];
			asm volatile("movb %0, %%es:(%1)" ::"r"(data), "r"(buf + i));
		}
		else
			break;
	}
	pcb[current].regs.eax = i;
}

void syscallReadFile(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int ret = 0;
	int sel = tf->ds;
	int fd = tf->ecx;
	uint8_t *destBuffer = (uint8_t *)tf->edx;
	int size = tf->ebx;
	uint8_t buffer[1024];
	char character = 0;
	SuperBlock superBlock;
	Inode inode;
	int inodeOffset = 0;
	int offset = 0;
	int blockIndex = 0;
	int base = 0;

	asm volatile("movw %0, %%es" ::"m"(sel));

	ret = readSuperBlock(&superBlock);
	if(ret == -1) {
		pcb[current].regs.eax = 0;
		return;
	}
	inodeOffset = sysFcb[pcb[current].fcb[fd].index].inodeOffset;
	offset = sysFcb[pcb[current].fcb[fd].index].offset;
	diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);
	if(offset >= inode.size) {
		pcb[current].regs.eax = 0;
		return;
	}

	blockIndex = offset / superBlock.blockSize;
	base = offset % superBlock.blockSize;
	
	
	if(size > inode.size - offset)
		size = inode.size - offset;

	ret = readBlock(&superBlock, &inode, blockIndex, buffer);
	if(ret == -1) {
		sysFcb[pcb[current].fcb[fd].index].offset += i;
		pcb[current].regs.eax = i;
		return;
	}
	blockIndex++;
	while (i < size) {
		character = buffer[(base + i) % superBlock.blockSize];
		asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(destBuffer + i));
		i++;
		if((base + i) % superBlock.blockSize == 0) {
			base = 0;
			ret = readBlock(&superBlock, &inode, blockIndex, buffer);
			if(ret == -1) {
				sysFcb[pcb[current].fcb[fd].index].offset += i;
				pcb[current].regs.eax = i;
				return;
			}
			blockIndex++;
		}
	}

	sysFcb[pcb[current].fcb[fd].index].offset += i;
	pcb[current].regs.eax = i;
}

void syscallReadDir(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int ret = 0;
	int pos = 0;
	int fd = tf->ecx;
	uint16_t data = 0;
	SuperBlock superBlock;
	Inode inode;
	int inodeOffset = 0;
	int dirIndex = 2;
	DirEntry dirEntry;
	
	
	ret = readSuperBlock(&superBlock);
	if(ret == -1) {
		pcb[current].regs.eax = 0;
		return;
	}
	inodeOffset = sysFcb[pcb[current].fcb[fd].index].inodeOffset;
	diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);
	if(inode.type != DIRECTORY_TYPE)
		return;
	
	while (getDirEntry(&superBlock, &inode, dirIndex, &dirEntry) == 0) {
        dirIndex ++;
		if(dirEntry.inode == 0)
			continue;
		i = 0;
		while(dirEntry.name[i] != 0) {
			data = dirEntry.name[i] | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80){
				displayRow++;
				displayCol = 0;
				if (displayRow == 25){
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
			i++;
		}
		data = ' ' | (0x0c << 8);
		pos = (80 * displayRow + displayCol) * 2;
		asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
		displayCol++;
		if (displayCol == 80){
			displayRow++;
			displayCol = 0;
			if (displayRow == 25){
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
    }
	displayRow++;
	displayCol = 0;
	if (displayRow == 25){
		displayRow = 24;
		displayCol = 0;
		scrollScreen();
	}
	updateCursor(displayRow, displayCol);
	
	pcb[current].regs.eax = dirIndex;
}

void syscallFork(struct TrapFrame *tf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD) {
			break;
		}
	}
	if (i != MAX_PCB_NUM) {
		pcb[i].state = STATE_PREPARING;

		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i + 1) * 0x100000) = *(uint8_t *)(j + (current + 1) * 0x100000);
		}
		disableInterrupt();

		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		pcb[i].regs.ss = USEL(2 + i * 2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1 + i * 2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2 + i * 2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		// TODO in lab5
		for (int k = 4; k < MAX_FCB_NUM; k++) {
			pcb[i].fcb[k].state = pcb[current].fcb[k].state;
			pcb[i].fcb[k].index = pcb[current].fcb[k].index;
			pcb[i].fcb[k].flags = pcb[current].fcb[k].flags;
			if(pcb[current].fcb[k].state == 1) {
				sysFcb[pcb[current].fcb[k].index].linkCount++;
			}
		}
		/*XXX set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct TrapFrame *tf) {
	int sel = tf->ds;
	char *str = (char *)tf->ecx;
	char tmp[128];
	int i = 0;
	char character = 0;
	int ret = 0;
	uint32_t entry = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	while (character != 0) {
		tmp[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	}
	tmp[i] = 0;

	ret = loadElf(tmp, (current + 1) * 0x100000, &entry);
	if (ret == -1) {
		tf->eax = -1;
		return;
	}
	tf->eip = entry;
	return;
}

void syscallSleep(struct TrapFrame *tf) {
	if (tf->ecx == 0) {
		return;
	}
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = tf->ecx;
		asm volatile("int $0x20");
		return;
	}
	return;
}

void syscallExit(struct TrapFrame *tf) {
	pcb[current].state = STATE_DEAD;
	// TODO in lab5
	int i = 0;
	int index = 0;
	for(i = 0; i < MAX_FCB_NUM; i++) {
		if(pcb[current].fcb[i].state == 1) {
			index = pcb[current].fcb[i].index;
			sysFcb[index].linkCount--;
			if(sysFcb[index].linkCount == 0) {
				sysFcb[index].state = 0;
				sysFcb[index].inodeOffset = 0;
				sysFcb[index].offset = 0;
			}
			pcb[current].fcb[i].state = 0;
			pcb[current].fcb[i].index = -1;
			pcb[current].fcb[i].flags = 0;
		}
	}
	
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct TrapFrame *tf) {
	switch(tf->ecx) {
		case SEM_INIT:
			syscallSemInit(tf);
			break;
		case SEM_WAIT:
			syscallSemWait(tf);
			break;
		case SEM_POST:
			syscallSemPost(tf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(tf);
			break;
		default:break;
	}
}

void syscallSemInit(struct TrapFrame *tf) {
	// TODO in lab4
	for(int i = 0; i < MAX_SEM_NUM; i++) {
		if(sem[i].state == 0) {
			sem[i].state = 1;
			sem[i].value = (int)tf->edx;
			pcb[current].regs.eax = i;
			return;
		}
	}
	pcb[current].regs.eax = -1;
}

void syscallSemWait(struct TrapFrame *tf) {
	// TODO in lab4
	int index = (int)tf->edx;
	if(sem[index].state == 1) {
		sem[index].value--;
		pcb[current].regs.eax = 0;
		if(sem[index].value < 0) {
			// block current process
			pcb[current].blocked.next = sem[index].pcb.next;
			pcb[current].blocked.prev = &(sem[index].pcb);
			sem[index].pcb.next = &(pcb[current].blocked);
			(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
			pcb[current].state = STATE_BLOCKED;
			asm volatile("int $0x20");
		}
	}
	else
		pcb[current].regs.eax = -1;
}

void syscallSemPost(struct TrapFrame *tf) {
	// TODO in lab4
	ProcessTable *pt = NULL;
	int index = (int)tf->edx;
	if(sem[index].state == 1) {
		sem[index].value++;
		pcb[current].regs.eax = 0;
		if(sem[index].value <= 0) {
			// wake up a process
			pt = (ProcessTable*)((uint32_t)(sem[index].pcb.prev) -
				(uint32_t)&(((ProcessTable*)0)->blocked));
			sem[index].pcb.prev = (sem[index].pcb.prev)->prev;
			(sem[index].pcb.prev)->next = &(sem[index].pcb);
			pt->state = STATE_RUNNABLE;
		}
	}
	else
		pcb[current].regs.eax = -1;
}

void syscallSemDestroy(struct TrapFrame *tf) {
	// TODO in lab4
	int index = (int)tf->edx;
	if(sem[index].state == 1) {
		sem[index].state = 0;
		pcb[current].regs.eax = 0;
	}
	else
		pcb[current].regs.eax = -1;
}

void syscallGetPid(struct TrapFrame *tf) {
	pcb[current].regs.eax = current;
	return;
}

void syscallOpen(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int j = 0;
	int k = 0;
	int ret = 0;
	int sel = tf->ds;
	int flags = tf->edx;
	char *str = (char *)tf->ecx;
	char path[128];
	char character = 0;
	SuperBlock superBlock;
	Inode destInode;
	int destInodeOffset = 0;

	asm volatile("movw %0, %%es" ::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	while (character != 0) {
		path[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	}
	path[i] = 0;


	ret = readSuperBlock(&superBlock);
	if(ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	ret = readInode(&superBlock, &destInode, &destInodeOffset, path);
	if (ret == -1) {
		if((flags & O_CREATE) == O_CREATE) {
			if((flags & O_DIRECTORY) == O_DIRECTORY)
				ret = mkdir(&superBlock, path, &destInode, &destInodeOffset);
			else
				ret = createFile(&superBlock, path, &destInode, &destInodeOffset);
		}
	}
	else {
		if((flags & O_DIRECTORY) == O_DIRECTORY && destInode.type != DIRECTORY_TYPE) {
			pcb[current].regs.eax = -1;
			return;
		}
	}
	if(ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}

	if(destInodeOffset == dev[STD_OUT].inodeOffset) {
		pcb[current].regs.eax = STD_OUT;
		return;
	}
	else if(destInodeOffset == dev[STD_IN].inodeOffset) {
		pcb[current].regs.eax = STD_IN;
		return;
	}
	else if(destInodeOffset == dev[SH_MEM].inodeOffset) {
		pcb[current].regs.eax = SH_MEM;
		return;
	}

	for (j = 4; j < MAX_SYSFCB_NUM; j++) {
		if(sysFcb[j].state == 0) {
			sysFcb[j].state = 1;
			sysFcb[j].inodeOffset = destInodeOffset;
			sysFcb[j].linkCount = 0;
			sysFcb[j].offset = 0;
			break;
		}
	}
	if(j == MAX_SYSFCB_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	for (k = 4; k < MAX_FCB_NUM; k++) {
		if(pcb[current].fcb[k].state == 0) {
			pcb[current].fcb[k].state = 1;
			pcb[current].fcb[k].index = j;
			sysFcb[j].linkCount++;
			pcb[current].fcb[k].flags = flags;
			pcb[current].regs.eax = k;
			return;
		}
	}

	sysFcb[j].state = 0;
	pcb[current].regs.eax = -1;
	return;
}

void syscallLseek(struct TrapFrame *tf) {
	// TODO in lab5
	int fd = tf->ecx;
	int value = tf->edx;
	int whence = tf->ebx;
	int inodeOffset = 0;
	Inode inode;

	if(pcb[current].fcb[fd].state == 0) {
		pcb[current].regs.eax = 0;
		return;
	}
	inodeOffset = sysFcb[pcb[current].fcb[fd].index].inodeOffset;
	diskRead((void *)&inode, sizeof(Inode), 1, inodeOffset);

	if(whence == SEEK_SET) {
		if(value >= 0)
			sysFcb[pcb[current].fcb[fd].index].offset = value;
	}
	else if(whence == SEEK_CUR)
		sysFcb[pcb[current].fcb[fd].index].offset += value;
	else if (whence == SEEK_END)
		sysFcb[pcb[current].fcb[fd].index].offset = inode.size + value;

	pcb[current].regs.eax = sysFcb[pcb[current].fcb[fd].index].offset;
}

void syscallClose(struct TrapFrame *tf) {
	// TODO in lab5
	int fd = tf->ecx;
	int index = 0;

	if(pcb[current].fcb[fd].state == 0) {
		pcb[current].regs.eax = -1;
		return;
	}

	index = pcb[current].fcb[fd].index;
	sysFcb[index].linkCount--;
	if(sysFcb[index].linkCount == 0) {
		sysFcb[index].state = 0;
		sysFcb[index].inodeOffset = 0;
		sysFcb[index].offset = 0;
	}
	pcb[current].fcb[fd].state = 0;
	pcb[current].fcb[fd].index = -1;
	pcb[current].fcb[fd].flags = 0;
	
	pcb[current].regs.eax = 0;
	return;
}

void syscallRemove(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int ret = 0;
	int sel = tf->ds;
	char *str = (char *)tf->ecx;
	char path[128];
	char character = 0;
	SuperBlock superBlock;
	Inode destInode;
	int destInodeOffset = 0;
	
	asm volatile("movw %0, %%es" ::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	while (character != 0) {
		path[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	}
	path[i] = 0;

	ret = readSuperBlock(&superBlock);
	if(ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	ret = readInode(&superBlock, &destInode, &destInodeOffset, path);
	if (ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}

	// delete entries in fcb and sysFcb (?)
	if(destInode.type == DIRECTORY_TYPE)
		ret = rmdir(path);
	else
		ret = rm(path);

	if(ret == -1)
		pcb[current].regs.eax = -1;
	else
		pcb[current].regs.eax = 0;
	return;
}

void syscallCreate(struct TrapFrame *tf) {
	// TODO in lab5
	int i = 0;
	int ret = 0;
	int sel = tf->ds;
	char *str = (char *)tf->ecx;
	char path[128];
	char character = 0;
	int length = 0;
	SuperBlock superBlock;
	Inode destInode;
	int destInodeOffset = 0;
	
	asm volatile("movw %0, %%es" ::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	while (character != 0) {
		path[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	}
	path[i] = 0;
	
	
	ret = readSuperBlock(&superBlock);
	if(ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	ret = readInode(&superBlock, &destInode, &destInodeOffset, path);
	if (ret != -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	
	length = stringLen(path);
	if(path[length - 1] == '/')
		ret = mkdir(&superBlock, path, &destInode, &destInodeOffset);
	else
		ret = createFile(&superBlock, path, &destInode, &destInodeOffset);
	if(ret == -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	
	pcb[current].regs.eax = 0;
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}
