Lab 4: Preemptive Multitasking实习报告
===================
1100016639 信息科学技术学院 吕鑫

---

目录
-------------
<!--toc-->


----
总体概述
-------------

// TODO
 




----
完成情况
-------------------


### 任务完成列表

|Exercise 1|Exercise 2|Exercise 3|Exercise 4|Exercise 5|Exercise 6|
|:--:|:--:|:--:|:--:|:--:|:--:|
|√   | √  | √  | √  | √  | √  |


|Exercise 7|Exercise 8| Challenge | Exercise 9|Exercise 10|Exercise 11|
|:--:|:--:|:--:|:--:|:--:|:--:|
|√   | √  | √  | √  | √  |√  |


----
### Part A: Multiprocessor Support and Cooperative Multitasking

#### Exercise 1
> Implement `mmio_map_region` in `kern/pmap.c`. To see how this is used, look at the beginning of `lapic_init` in `kern/lapic.c`. You'll have to do the next exercise, too, before the tests for `mmio_map_region` will run.

```
void *
mmio_map_region(physaddr_t pa, size_t size)
{

	static uintptr_t base = MMIOBASE;
	size = ROUNDUP(size, PGSIZE);
	if (size > MMIOLIM)
		panic("mmip_map_region: size overflows MMIOLIM");
	boot_map_region(kern_pgdir, base, size, pa, PTE_W|PTE_PCD|PTE_PWT);
	base += size;
	return (void *) (base - size);
}
```

---

#### Exercise 2
> Read `boot_aps()` and `mp_main()` in `kern/init.c`, and the assembly code in `kern/mpentry.S`. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of `page_init()` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated `check_page_free_list()` test (but might fail the updated `check_kern_pgdir()` test, which we will fix soon).

在分配前面`[PGSIZE, npages_basemem * PGSIZE)`时，利用`pa2page`判断是否为`MPENTRY_ADDR`所在的一页即可。

```
    size_t i;
	struct PageInfo *pp = pa2page(MPENTRY_PADDR);
    // [PGSIZE, npages_basemem * PGSIZE) is free
    for (i = 1; i < npages_basemem; ++i) {
    	// mark MPENTRY_ADDR as in use
    	if (pages + i == pp)
    		continue;
        pages[i].pp_ref = 0;
        pages[i].pp_link = page_free_list;
        page_free_list = &pages[i];
    }
```

----
#### Question 1
> Compare `kern/mpentry.S` side by side with `boot/boot.S`. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel, what is the purpose of macro `MPBOOTPHYS`? Why is it necessary in `kern/mpentry.S` but not in `boot/boot.S`? In other words, what could go wrong if it were omitted in `kern/mpentry.S`? 

题目里已经提示，`mpentry.S`编译链接都是在`KERNBASE`上的内存地址上进行的，此时还处于**实模式**中，`mpentry.S`中的符号地址全部处于高地址。为了使代码能在`MPENTRY_PADDR`的低地址成功运行，需要在`mpentry.S`中需要将这些高地址转变为低地址，这就是`MPBOOTPHYS`这个宏的作用。

-----
#### Exercise 4
> Modify `mem_init_mp()` (in `kern/pmap.c`) to map per-CPU stacks starting at `KSTACKTOP`, as shown in `inc/memlayout.h`. The size of each stack is `KSTKSIZE` bytes plus `KSTKGAP` bytes of unmapped guard pages. Your code should pass the new check in `check_kern_pgdir()`.
```
static void
mem_init_mp(void)
{
    int i;
    uint32_t kstacktop_i;
    for (i = 0; i < NCPU; ++i) {
        kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
        boot_map_region(kern_pgdir, kstacktop_i - KSTKSIZE, KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W | PTE_P);
    }


}
```

-----
#### Exercise 5
> Apply the big kernel lock as described above, by calling `lock_kernel()` and `unlock_kernel()` at the proper locations.

很简单，按照lab指示，在相应位置加上锁即可。

----
#### Question 2
> It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

中断发生时会自动压栈，而这时候还没有取得锁，若多个CPU同时发生中断，共享内核栈将会出错。

----
#### Exercise 6
> Implement round-robin scheduling in `sched_yield()` as described above. Don't forget to modify `syscall()` to dispatch `sys_yield()`.

```
void
sched_yield(void)
{
	struct Env *idle;
	int i, env_idx;
	if (curenv)
		env_idx = (ENVX(curenv->env_id) + 1) % NENV;
	else
		env_idx = 0;
	for (i = 0; i < NENV; ++i) {
		idle = &envs[env_idx];
		if (idle->env_status == ENV_RUNNABLE) {
			env_run(idle);
		}
		env_idx = (env_idx + 1) % NENV;
	}

	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);

	// sched_halt never returns
	sched_halt();
}
```

----
#### Question 3
> In your implementation of `env_run()` you should have called `lcr3()`. Before and after the call to `lcr3()`, your code makes references (at least it should) to the variable e, the argument to `env_run.` Upon loading the `%cr3` register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

这是由于我们在`mem_init()`中对`envs`数组进行了静态映射，而每个`env`的`pgdir`都是拷贝自`kern_pgdir`的，自然也做了这部分映射了，因此寻址不会有问题。

----
#### Question 4
> Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

进程上下文切换时，保存现场是必要的，否则切换回来时，CPU就不知道之前的状态，也不知道从何处开始继续执行之前的代码了。

保存现场的过程是在发生中断的时候完成的，在`trap()`中拷贝了一份Trapframe：

```
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
```

----
#### Exercise 7
> Implement the system calls described above in `kern/syscall.c`. You will need to use various functions in `kern/pmap.c` and `kern/env.c`, particularly `envid2env()`. For now, whenever you call `envid2env()`, pass 1 in the checkperm parameter. Be sure you check for any invalid system call arguments, returning `-E_INVAL` in that case. Test your JOS kernel with `user/dumbfork` and make sure it works before proceeding.

##### 1. `sys_exofork()`:

```
static envid_t
sys_exofork(void)
{
	struct Env *e;
	int r;
	r = env_alloc(&e, curenv->env_id);
	if (r < 0)
		return r;
	e->env_status = ENV_NOT_RUNNABLE;
	e->env_tf = curenv->env_tf;
	e->env_tf.tf_regs.reg_eax = 0;
	return e->env_id;
}
```

##### 2. `sys_env_set_status()`:

```
static int
sys_env_set_status(envid_t envid, int status)
{
	struct Env *e;
	int r;
	r = envid2env(envid, &e, 1);
	if (r < 0)
		return r;
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;
	e->env_status = status;
	return 0;
}
```
##### 3. `sys_page_alloc()`:

```
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	struct Env *e;
	struct PageInfo *pp;
	int r, mask;
	r = envid2env(envid, &e, 1);
	if (r < 0)
		return r;
	if ((uint32_t) va >= UTOP || (uint32_t) va % PGSIZE)
		return -E_INVAL;
	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || perm & ~PTE_SYSCALL)
		return -E_INVAL;
	pp = page_alloc(ALLOC_ZERO);
	if (!pp)
		return -E_NO_MEM;
	r = page_insert(e->env_pgdir, pp, va, perm);
	if (r < 0) {
		page_free(pp);
		return r;
	}
	return 0;
}
```

##### 4. `sys_page_map()`:

```
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	struct Env *src_e;
	struct Env *dst_e;
	struct PageInfo *pp;
	pte_t *pte_ptr;
	int r;
	r = envid2env(srcenvid, &src_e, 1);
	if (r < 0)
		return r;
	r = envid2env(dstenvid, &dst_e, 1);
	if (r < 0)
		return r;
	if ((uint32_t) srcva >= UTOP || (uint32_t) dstva >= UTOP || (uint32_t) srcva % PGSIZE || (uint32_t) dstva % PGSIZE)
		return -E_INVAL;
	pp = page_lookup(src_e->env_pgdir, srcva, &pte_ptr);
	if (!pp)
		return -E_INVAL;
	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || perm & ~PTE_SYSCALL)
		return -E_INVAL;
	if ((perm & PTE_W) && !((*pte_ptr) & PTE_W))
		return -E_INVAL;
	r = page_insert(dst_e->env_pgdir, pp, dstva, perm);
	if (r < 0)
		return r;
	return 0;
}
```

##### 5. `sys_page_unmap()`:

```
static int
sys_page_unmap(envid_t envid, void *va)
{
	struct Env *e;
	int r;
	r = envid2env(envid, &e, 1);
	if (r < 0)
		return r;
	if ((uint32_t) va >= UTOP || (uint32_t) va % PGSIZE)
		return -E_INVAL;
	page_remove(e->env_pgdir, va);
	return 0;
}
```

----
### Part B: Copy-on-Write Fork

#### Exercise 8
> Implement the `sys_env_set_pgfault_upcall` system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call.

```
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	struct Env *e;
	int r;
	r = envid2env(envid, &e, 1);
	if (r < 0)
		return r;
	e->env_pgfault_upcall = func;
	return 0;
}
```

----

#### Exercise 9
> Implement the code in `page_fault_handler` in `kern/trap.c` required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)

```
void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	struct UTrapframe *utf;
	if (curenv->env_pgfault_upcall) {
		if (tf->tf_esp <= UXSTACKTOP - 1 && tf->tf_esp >= UXSTACKTOP - PGSIZE) {
			utf = (struct UTrapframe *) (tf->tf_esp - 4 - sizeof(struct UTrapframe));
			user_mem_assert(curenv, (void *) utf, sizeof(struct UTrapframe) + 4, PTE_W|PTE_U);
		} else {
			utf = (struct UTrapframe *) (UXSTACKTOP - sizeof(struct UTrapframe));
			user_mem_assert(curenv, (void *) utf, sizeof(struct UTrapframe), PTE_W|PTE_U);

		}
		utf->utf_regs = tf->tf_regs;
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;
		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_err;
		tf->tf_esp = (uintptr_t) utf;
		tf->tf_eip = (uintptr_t) curenv->env_pgfault_upcall;

		env_run(curenv);
	}

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
```

----

#### Exercise 10
> Implement the `_pgfault_upcall` routine in `lib/pfentry.S`. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP.

```
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.
	movl 0x30(%esp), %eax
	subl $4, %eax
	movl %eax, 0x30(%esp)
	movl 0x28(%esp), %ebx
	movl %ebx, (%eax)
	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	addl $0x8, %esp
	popal
	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	addl $0x4, %esp
	popfl
	// Switch back to the adjusted trap-time stack.
	popl %esp
	// Return to re-execute the instruction that faulted.
	ret
```

----

#### Exercise 11
> Finish `set_pgfault_handler()` in `lib/pgfault.c`.

```
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		r = sys_page_alloc(thisenv->env_id, (void *) (UXSTACKTOP - PGSIZE), PTE_W|PTE_U|PTE_P);
		if (r < 0)
			panic("set_pgfault_handler error");
		r = sys_env_set_pgfault_upcall(thisenv->env_id, (void *)_pgfault_upcall);
		if (r < 0)
			panic("set_pgfault_handler error");

	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}

```

----

#### Exercise 12

`fork()`:

```
envid_t
fork(void)
{
	// LAB 4: Your code here.

	uint32_t i;
	int r;
	envid_t envid;
	extern void _pgfault_upcall(void);
	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if (envid < 0)
		panic("fork: sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child
		// Fix thisenv for child env
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// We're the parent
	// Duplicate the pages mapping first
	// We must check uvpd FIRST!!! Otherwise uvpt would page fault!!!
	// It takes me a long time to debug this stupid bug!!!

	for (i = 0; i < PGNUM(UTOP); ++i) {
		if ((uvpd[PDX(i << 12)] & PTE_P) && (uvpt[i] & PTE_P) && (uvpt[i] & PTE_U)) {
			duppage(envid , i);
		}
	}

	// Allocate the space for user exception stack
	if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("fork: sys_page_alloc: %e", r);
	// Set page fault upcall
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("fork: sys_env_set_pgfault_upcall: %e", r);
	// Mark the child as runnable
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork: sys_env_set_status: %e", r);
	return envid;

}
```

`duppage()`:

```
static int
duppage(envid_t envid, unsigned pn)
{
	// do not dup exception stack
	if (pn * PGSIZE == UXSTACKTOP - PGSIZE) return 0;

	int r;
	void * addr = (void *)(pn * PGSIZE);
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
		// cow
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0) panic("duppage sys_page_map error : %e\n", r);
		
		r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0) panic("duppage sys_page_map error : %e\n", r);
	} else {
		// read only
		r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U);
		if (r < 0) panic("duppage sys_page_map error : %e\n", r);
	}

	return 0;
}
```

`pgfault()`:
```
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	if ((err & FEC_WR) == 0)
		panic("pgfault, not a write fault. va: 0x%x\n", addr);

	uint32_t uaddr = (uint32_t) addr;
	if ((uvpd[PDX(addr)] & PTE_P) == 0 || (uvpt[uaddr / PGSIZE] & PTE_COW) == 0) {
		panic("pgfault, not a copy-on-write page. va: 0x%x\n", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	r = sys_page_alloc(0, (void *)PFTEMP, PTE_W | PTE_U | PTE_P);
	if (r < 0) panic("pgfault, sys_page_alloc error : %e\n", r);
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy(PFTEMP, addr, PGSIZE);	
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P);
	if (r < 0) panic("pgfault, sys_page_map error : %e\n", r);
	return;
}
```

----