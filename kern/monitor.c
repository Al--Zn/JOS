// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "showmappings", "Display information about page mappings between [start_va, end_va]", mon_showmappings },
	{ "setperm", "Set permission for the given virtual addr", mon_setperm },
	{ "dump", "Dump the content of the given region", mon_dump}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{

	int *ebp, *eip, i;
	int args[5];
	struct Eipdebuginfo info;
	cprintf("Stack backtrace:\n");
	ebp = (int*)read_ebp();
	eip = ebp + 1;
	for (i = 1; i <= 5; ++i) {
		args[i-1] = *(eip + i);
	}
	while (ebp) {
		debuginfo_eip(*eip, &info);
		cprintf("  ebp %08x  eip %08x  args", ebp, *eip);
		for (i = 0; i < 5; ++i) {
			cprintf(" %08x", args[i]);
		}
		cprintf("\n");
		cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, *eip - info.eip_fn_addr);
		ebp = (int*)(*ebp);
		eip = ebp + 1;
		for (i = 1; i <= 5; ++i) {
			args[i-1] = *(eip + i);
		}
	}
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	size_t i, j, len;
	uintptr_t start_va, end_va, va;
	pte_t *pteptr;
	// check the parameter num
	if (argc != 3)
		goto showmappings_bad;
	// check start_va
	start_va = 0;
	len = strlen(argv[1]);
	if (len > 10 || len < 3 || !(argv[1][0] == '0' && argv[1][1] == 'x'))
		goto showmappings_bad;
	for (i = 2; i < 10 && i < len; ++i) {
		start_va = start_va * 16 + argv[1][i];
		if (argv[1][i] >= 'a' && argv[1][i] <= 'z')
			start_va -= 'a' - 10;
		else if (argv[1][i] >= 'A' && argv[1][i] <= 'Z')
			start_va -= 'A' - 10;
		else if (argv[1][i] >= '0' && argv[1][i] <= '9')
			start_va -= '0'; 
	}

	// check end_va
	end_va = 0;
	len = strlen(argv[2]);
	if (len > 10 || len < 3 || !(argv[2][0] == '0' && argv[2][1] == 'x'))
		goto showmappings_bad;
	for (i = 2; i < 10 && i < len; ++i) {
		end_va = end_va * 16 + argv[2][i];
		if (argv[2][i] >= 'a' && argv[2][i] <= 'z')
			end_va -= 'a' - 10;
		else if (argv[2][i] >= 'A' && argv[2][i] <= 'Z')
			end_va -= 'A' - 10;
		else if (argv[2][i] >= '0' && argv[2][i] <= '9')
			end_va -= '0'; 
	}

	if (start_va > end_va)
		goto showmappings_bad;
	show_map_region(start_va, end_va);
	return 0;
showmappings_bad:
	cprintf("showmappings: \033[31millegal arguments\033[0m\n");
	cprintf("usage: showmappings start_va end_va (Eg. showmappings 0x300 0x500)\n");
	return -1;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va;
	int perm;

	if (argc != 3)
		goto setperm_bad;

	va = strtol(argv[1], NULL, 0);
	perm = strtol(argv[2], NULL, 0);

	if (perm >= 0x1000)
		goto setperm_bad;

	setperm(va, perm);
	return 0;
setperm_bad:
	cprintf("setperm: \033[31millegal arguments\033[0m\n");
	cprintf("usage: setperm va perm (Eg. setperm 0xf0000000 0x001), /"
	         "the value of each permission is defined in 'inc/mmu.h\n'");
	return -1;
}

int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t start_va, end_va;
	physaddr_t start_pa, end_pa;
	if (argc != 4)
		goto dump_bad;
	if (strlen(argv[1]) != 2 || argv[1][0] != '-' || !(argv[1][1] == 'v' || argv[1][1] == 'p'))
		goto dump_bad;
	if (argv[1][1] == 'v') {
		start_va = strtol(argv[2], NULL, 0);
		end_va = strtol(argv[3], NULL, 0);
		dump_virtaddr(start_va, end_va);
	} else {
		start_pa = strtol(argv[2], NULL, 0);
		end_pa = strtol(argv[3], NULL, 0);
		dump_physaddr(start_pa, end_pa);
	}

	return 0;
dump_bad:
	cprintf("dump: \033[31millegal arguments\033[0m\n");
	cprintf("usage: dump -{v,p} start_addr end_addr. -v for virtual address, -p for physical address\n");
	return -1;
}
/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

