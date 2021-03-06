/* Real Mode Hello World */
/*
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0x7d00, %ax
	movw %ax, %sp # setting stack pointer to 0x7d00
	pushw $13 # pushing the size to print into stack
	pushw $message # pushing the address of message into stack
	callw displayStr # calling the display function
loop:
	jmp loop

message:
	.string "Hello, World!\n\0"

displayStr:
	pushw %bp
	movw 4(%esp), %ax
	movw %ax, %bp
	movw 6(%esp), %cx
	movw $0x1301, %ax
	movw $0x000c, %bx
	movw $0x0000, %dx
	int $0x10
	popw %bp
	ret
*/


/* Protected Mode Hello World */
.code16
start:
	cli
	inb $0x92, %al
	orb $0x02, %al
	outb %al, $0x92
	data32 addr32 lgdt gdtDesc
	movl %cr0, %eax
	orb $0x01, %al
	movl %eax, %cr0
	data32 ljmp $0x08, $start32

.code32
start32:
	movw $(2<<3), %ax
	movw %ax, %ds
	movw %ax, %ss
	movw %ax, %es
	movw %ax, %fs
	movw $(3<<3), %ax
	movw %ax, %gs
	movl $(128<<20), %eax
	movl %eax, %esp # setting stack pointer to 0x7d00000
	jmp bootMain

gdt:
	.word 0,0			
	.byte 0,0,0,0
	
	.word 0xffff,0			#Code Segment
	.byte 0,0x9a,0xcf,0

	.word 0xffff,0			#Data Segment
	.byte 0,0x92,0xcf,0

	.word 0xffff,0x8000		#Video Segment
	.byte 0x0b,0x92,0xcf,0

gdtDesc:
	.word (gdtDesc - gdt - 1)
	.long gdt
