#include "common.h"
#include "x86.h"
#include "device.h"
#include "fs.h"

void kEntry(void) {

	// Interruption is disabled in bootloader

	initSerial(); // initialize serial port
	initIdt(); // initialize idt
	initIntr(); // iniialize 8259a
	initSeg(); // initialize gdt, tss
	initVga(); // initialize vga device
	initTimer(); // initialize timer device
	initKeyTable(); // initialize keyboard device
	initDev(); // initialize device list
	initFS(); // initialize file system
	initSem(); // initialize semaphore list
	initProc(); // initialize pcb & load user program
}
