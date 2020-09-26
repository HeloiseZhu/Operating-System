#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

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
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallFork(tf);
			break; // for SYS_FORK
		case 2:
			syscallExec(tf);
			break; // for SYS_EXEC
		case 3:
			syscallSleep(tf);
			break; // for SYS_SLEEP
		case 4:
			syscallExit(tf);
			break; // for SYS_EXIT
		default:break;
	}
}

void timerHandle(struct TrapFrame *tf) {
	// TODO in lab3
	for(int i = 0; i < MAX_PCB_NUM; i++) {
		if(i != current && pcb[i].state == STATE_BLOCKED) {
			pcb[i].sleepTime--;
			if(pcb[i].sleepTime <= 0) {
				pcb[i].state = STATE_RUNNABLE;
			}
		}
	}
	
	if(pcb[current].state == STATE_RUNNING) {
		if(pcb[current].timeCount != MAX_TIME_COUNT) {
			pcb[current].timeCount++;
			return;
		}
		else {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 1;
		}
	}
	
	// find runnable process
	int next = current;
	for(int i = (current+1)%MAX_PCB_NUM; i != current; i = (i+1)%MAX_PCB_NUM) {
		if(i != 0 && pcb[i].state == STATE_RUNNABLE) {
			next = i;
			break;
		}
	}
	if(next == current) {
		if(pcb[current].state == STATE_RUNNABLE) {
			pcb[current].state = STATE_RUNNING;
			pcb[current].timeCount = 1;
			return;
		}
		else {
			next = 0;
		}
	}
	
	// switch process
	current = next;
	pcb[current].state = STATE_RUNNING;
	pcb[current].timeCount = 1;
	uint32_t tmpStackTop = pcb[current].stackTop;
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

void keyboardHandle(struct TrapFrame *tf) {
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0)
		return;
	putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	return;
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
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

void syscallFork(struct TrapFrame *tf) {
	// TODO in lab3
	// find available pcb
	int idx = -1;
	for(int i = 0; i < MAX_PCB_NUM; i++) {
		if(pcb[i].state == STATE_DEAD) {
			idx = i;
			break;
		}
	}
	if(idx == -1) {	// fail
		// set return value
		pcb[current].regs.eax = -1;
	}		
	else {
		// copy memory space
		enableInterrupt();
		for (int j = 0; j < 0x100000; j++) {
			*(uint8_t *)((idx + 1) * 0x100000 + j) = *(uint8_t *)((current + 1) * 0x100000 + j);
			asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		}
		disableInterrupt();
		
		// set pcb of child process
		// kernel stack
		for(int i = 0; i < MAX_STACK_SIZE; i++) {
			pcb[idx].stack[i] = pcb[current].stack[i];
		}
		// (?) (child process)original stacktop - current stacktop = (father process)original stacktop - current stacktop
		pcb[idx].stackTop = (uint32_t)&(pcb[idx].regs)/* - (uint32_t)&(pcb[current].regs) + pcb[current].stackTop*/; 
		pcb[idx].prevStackTop = (uint32_t)&(pcb[idx].stackTop)/* - (uint32_t)&(pcb[current].stackTop) + pcb[current].prevStackTop*/;
		// trap frame
		pcb[idx].regs.cs = USEL(1 + 2 * idx);
		pcb[idx].regs.ss = USEL(2 + 2 * idx);
		pcb[idx].regs.gs = USEL(2 + 2 * idx);
		pcb[idx].regs.fs = USEL(2 + 2 * idx);
		pcb[idx].regs.es = USEL(2 + 2 * idx);
		pcb[idx].regs.ds = USEL(2 + 2 * idx);
		pcb[idx].regs.edi = pcb[current].regs.edi;
		pcb[idx].regs.esi = pcb[current].regs.esi;
		pcb[idx].regs.ebp = pcb[current].regs.ebp; // ?
		pcb[idx].regs.xxx = pcb[current].regs.xxx;
		pcb[idx].regs.ebx = pcb[current].regs.ebx;
		pcb[idx].regs.edx = pcb[current].regs.edx;
		pcb[idx].regs.ecx = pcb[current].regs.ecx;
		//pcb[idx].regs.eax = pcb[current].regs.eax;
		pcb[idx].regs.irq = pcb[current].regs.irq;
		pcb[idx].regs.error = pcb[current].regs.error;
		pcb[idx].regs.eip = pcb[current].regs.eip;
		pcb[idx].regs.eflags = pcb[current].regs.eflags;
		pcb[idx].regs.esp = pcb[current].regs.esp; // ?
		// other info
		pcb[idx].state = STATE_RUNNABLE;
		pcb[idx].timeCount = 0;
		pcb[idx].sleepTime = 0;
		pcb[idx].pid = idx;
		
		// set return value
		pcb[current].regs.eax = idx;
		pcb[idx].regs.eax = 0;
	}
}

void syscallExec(struct TrapFrame *tf) {
	// TODO in lab3
	// hint: ret = loadElf(tmp, (current + 1) * 0x100000, &entry);
	int sel = tf->ds;
	char *str = (char *)(tf->ecx);
	char tmp[128];
	char ch;
	int i = 0;
	uint32_t entry = 0;
	int ret = 0;

	asm volatile("movw %0, %%es"::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(ch):"r"(str + i));
	while(ch != '\0') {
		tmp[i] = ch;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(ch):"r"(str + i));
	}
	tmp[i] = '\0';
	
	ret = loadElf((const char*)tmp, (current + 1) * 0x100000, &entry);
	if(ret == -1) {
		pcb[current].regs.eax = -1;
	}
	else {
		pcb[current].regs.eip = entry;
	}
}

void syscallSleep(struct TrapFrame *tf) {
	// TODO in lab3
	if((int)tf->ecx <= 0) { // illegal input
		return;
	}
	else {
		pcb[current].sleepTime = tf->ecx;
		//pcb[current].timeCount = MAX_TIME_COUNT;
		pcb[current].state = STATE_BLOCKED;
		asm volatile("int $0x20");
	}
}

void syscallExit(struct TrapFrame *tf) {
	// TODO in lab3
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}
