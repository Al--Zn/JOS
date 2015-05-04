/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


#define TRAPELEMENT(name, num)						\
.data;									\
	.long name; 							\
.text;									\
TRAPHANDLER(name, num)

#define TRAPELEMENT_NOEC(name, num)					\
.data;									\
	.long name; 							\
.text;									\
TRAPHANDLER_NOEC(name, num)

#define TRAPELEMENT_NONE()						\
.data;								        \
	.long 0;								



.data
	.p2align 2
	.globl handlers
handlers:
.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPELEMENT_NOEC(trap0, 0)
TRAPELEMENT_NOEC(trap1, 1)
TRAPELEMENT_NOEC(trap2, 2)
TRAPELEMENT_NOEC(trap3, 3)
TRAPELEMENT_NOEC(trap4, 4)
TRAPELEMENT_NOEC(trap5, 5)
TRAPELEMENT_NOEC(trap6, 6)
TRAPELEMENT_NOEC(trap7, 7)
TRAPELEMENT(trap8, 8)
TRAPELEMENT_NONE()
TRAPELEMENT(trap10, 10)
TRAPELEMENT(trap11, 11)
TRAPELEMENT(trap12, 12)
TRAPELEMENT(trap13, 13)
TRAPELEMENT(trap14, 14)
TRAPELEMENT_NONE()
TRAPELEMENT_NOEC(trap16, 16)
TRAPELEMENT(trap17, 17)
TRAPELEMENT_NOEC(trap18, 18)
TRAPELEMENT_NOEC(trap19, 19)

TRAPHANDLER_NOEC(trap48, 48)
TRAPHANDLER_NOEC(irq0, 32)
TRAPHANDLER_NOEC(irq1, 33)
TRAPHANDLER_NOEC(irq4, 36)
TRAPHANDLER_NOEC(irq7, 39)
TRAPHANDLER_NOEC(irq14, 46)
TRAPHANDLER_NOEC(irq19, 51)



/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
	pushl %ds
	pushl %es
	pushal
	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap
