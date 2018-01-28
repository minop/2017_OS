#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/swap.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/swap.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

// pocitadlo na spomalenie counteru
static int pocitadlo = 0;

void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	
	extern void TH_DIVIDE(); 	SETGATE(idt[T_DIVIDE], 0, GD_KT, TH_DIVIDE, 0); 
	extern void TH_DEBUG(); 	SETGATE(idt[T_DEBUG], 0, GD_KT, TH_DEBUG, 0); 
	extern void TH_NMI(); 		SETGATE(idt[T_NMI], 0, GD_KT, TH_NMI, 0); 
	extern void TH_BRKPT(); 	SETGATE(idt[T_BRKPT], 0, GD_KT, TH_BRKPT, 3); 
	extern void TH_OFLOW(); 	SETGATE(idt[T_OFLOW], 0, GD_KT, TH_OFLOW, 0); 
	extern void TH_BOUND(); 	SETGATE(idt[T_BOUND], 0, GD_KT, TH_BOUND, 0); 
	extern void TH_ILLOP(); 	SETGATE(idt[T_ILLOP], 0, GD_KT, TH_ILLOP, 0); 
	extern void TH_DEVICE(); 	SETGATE(idt[T_DEVICE], 0, GD_KT, TH_DEVICE, 0); 
	extern void TH_DBLFLT(); 	SETGATE(idt[T_DBLFLT], 0, GD_KT, TH_DBLFLT, 0); 
	extern void TH_TSS(); 		SETGATE(idt[T_TSS], 0, GD_KT, TH_TSS, 0); 
	extern void TH_SEGNP(); 	SETGATE(idt[T_SEGNP], 0, GD_KT, TH_SEGNP, 0); 
	extern void TH_STACK(); 	SETGATE(idt[T_STACK], 0, GD_KT, TH_STACK, 0); 
	extern void TH_GPFLT(); 	SETGATE(idt[T_GPFLT], 0, GD_KT, TH_GPFLT, 0); 
	extern void TH_PGFLT(); 	SETGATE(idt[T_PGFLT], 0, GD_KT, TH_PGFLT, 0); 
	extern void TH_FPERR(); 	SETGATE(idt[T_FPERR], 0, GD_KT, TH_FPERR, 0); 
	extern void TH_ALIGN(); 	SETGATE(idt[T_ALIGN], 0, GD_KT, TH_ALIGN, 0); 
	extern void TH_MCHK(); 		SETGATE(idt[T_MCHK], 0, GD_KT, TH_MCHK, 0); 
	extern void TH_SIMDERR(); 	SETGATE(idt[T_SIMDERR], 0, GD_KT, TH_SIMDERR, 0); 
	extern void TH_SYSCALL(); 	SETGATE(idt[T_SYSCALL], 0, GD_KT, TH_SYSCALL, 3);
	extern void TH_IRQ_TIMER();	SETGATE(idt[IRQ_OFFSET+IRQ_TIMER], 0, GD_KT, TH_IRQ_TIMER, 0);
	extern void TH_IRQ_KBD();	SETGATE(idt[IRQ_OFFSET+IRQ_KBD], 0, GD_KT, TH_IRQ_KBD, 0);
	extern void TH_IRQ_2();		SETGATE(idt[IRQ_OFFSET+2], 0, GD_KT, TH_IRQ_2, 0);
	extern void TH_IRQ_3();		SETGATE(idt[IRQ_OFFSET+3], 0, GD_KT, TH_IRQ_3, 0);
	extern void TH_IRQ_SERIAL();	SETGATE(idt[IRQ_OFFSET+IRQ_SERIAL], 0, GD_KT, TH_IRQ_SERIAL, 0);
	extern void TH_IRQ_5();		SETGATE(idt[IRQ_OFFSET+5], 0, GD_KT, TH_IRQ_5, 0);
	extern void TH_IRQ_6();		SETGATE(idt[IRQ_OFFSET+6], 0, GD_KT, TH_IRQ_6, 0);
	extern void TH_IRQ_SPURIOUS();	SETGATE(idt[IRQ_OFFSET+IRQ_SPURIOUS], 0, GD_KT, TH_IRQ_SPURIOUS, 0);
	extern void TH_IRQ_8();		SETGATE(idt[IRQ_OFFSET+8], 0, GD_KT, TH_IRQ_8, 0);
	extern void TH_IRQ_9();		SETGATE(idt[IRQ_OFFSET+9], 0, GD_KT, TH_IRQ_9, 0);
	extern void TH_IRQ_10();	SETGATE(idt[IRQ_OFFSET+10], 0, GD_KT, TH_IRQ_10, 0);
	extern void TH_IRQ_11();	SETGATE(idt[IRQ_OFFSET+11], 0, GD_KT, TH_IRQ_11, 0);
	extern void TH_IRQ_12();	SETGATE(idt[IRQ_OFFSET+12], 0, GD_KT, TH_IRQ_12, 0);
	extern void TH_IRQ_13();	SETGATE(idt[IRQ_OFFSET+13], 0, GD_KT, TH_IRQ_13, 0);
	extern void TH_IRQ_IDE();	SETGATE(idt[IRQ_OFFSET+IRQ_IDE], 0, GD_KT, TH_IRQ_IDE, 0);
	extern void TH_IRQ_15();	SETGATE(idt[IRQ_OFFSET+15], 0, GD_KT, TH_IRQ_15, 0);
	extern void TH_IRQ_ERROR();	SETGATE(idt[IRQ_OFFSET+IRQ_ERROR], 0, GD_KT, TH_IRQ_ERROR, 0);

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
	//     from doing IO (0 is not the correct value!)
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpunum()*(KSTKSIZE + KSTKGAP); // ts.ts_esp0 = KSTACKTOP;
	thiscpu->cpu_ts.ts_ss0 = GD_KD; // ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate); // podla konzultacii s cviciacimi (iomb sa podla intel manualu nachadza na konci Taskstate segmentu)

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + cpunum()] = SEG16(STS_T32A, (uint32_t) &(thiscpu->cpu_ts), sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + cpunum()].sd_s = 0; // mozno tu treba zmenit hodnoty pre rozne cpu?

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + 8*cpunum());

	// Load the IDT
	lidt(&idt_pd);
	//DT (Load IDT register) loads the IDT register with the linear base address and limit values contained in the memory operand. This instruction can be executed only when the CPL is zero. It is normally used by the initialization logic of an operating system when creating an IDT. An operating system may also use it to change from one IDT to another.
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	int32_t navratovaH;	

	// podla cisla vynimky presmerujem jej spracovanie (inc/trap.h)
	// ked sa spracuje volanie myslim si, ze by som mal ukoncit funkciu, za switchom je totiz osetrenie nespracovaneho volania kam sa program aj po spravnom spracovani vzdy dostane... (ak je to zle tak sa to snad prejavi v buducnosti)
	switch(tf->tf_trapno) {
		case T_PGFLT: // page fault
			page_fault_handler(tf);
			return;

		case T_BRKPT: // breakpoint
			monitor(tf); // spustim monitor
			return;

		case T_SYSCALL: // systemove volanie
			navratovaH = syscall(	tf->tf_regs.reg_eax, /* v registri eax je cislo sys volania */\
						tf->tf_regs.reg_edx, \
						tf->tf_regs.reg_ecx, \
						tf->tf_regs.reg_ebx, \
						tf->tf_regs.reg_edi, \
						tf->tf_regs.reg_esi);
			// nahrat navratovu hodnotu do registra eax
			tf->tf_regs.reg_eax = navratovaH;		
			return;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	if(tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi(); // podla instrukcii v komentary

		// zmazem PTE_A bity na vsetkych strankach
		++pocitadlo;
		if(pocitadlo == MAXPERIODA) {
			// velmy spinavy trik, mazanie PTE_A flagov trva dlho (asi az tak, ze znovu nastane prerusenie od casovaca) a preto ho vykonavam len kazdych MAXPERIODA (16) preruseni
			clear_accessed_flags();
			pocitadlo = 0;
		}
		
		// preplanujem prostredia
		sched_yield();
	}

	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
		kbd_intr();
		return;
	}

	if(tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
		serial_intr();
		return;
	}

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();

		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;
	int i;
	struct Mapping *current;
	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	
	if((tf->tf_cs & 0x3) == 0) {
		// spodne dva bity 'code segment' registra su nulove => nachadzam sa v jadre
		panic("Page fault in kernel accesing va %08x\n", fault_va);
	}


	//zistim, ze ci fault_va pre curenv je niekde v poli swap_pages.

	for(i = 0; i < MAXSWAPPEDPAGES; i++){
		current = swap_pages[i];
		while(current != NULL){
			if(curenv->env_id == current->env_id && fault_va == current->va){
				//stranka je ulozena na disku. treba ju z neho nacitat.
				swap_page_from_disk((void*) fault_va, i, curenv->env_id);
			}
			current = current->next;
		}
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// It is convenient for our code which returns from a page fault
	// (lib/pfentry.S) to have one word of scratch space at the top of the
	// trap-time stack; it allows us to more easily restore the eip/esp. In
	// the non-recursive case, we don't have to worry about this because
	// the top of the regular user stack is free.  In the recursive case,
	// this means we have to leave an extra word between the current top of
	// the exception stack and the new stack frame because the exception
	// stack _is_ the trap-time stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.

	// page fault v user mode
	// existuje page fault upcall? (ak nie nebudem ho moct zavolat)
	if(curenv->env_pgfault_upcall != NULL) {
		// ked existuje upcall mam dve ulohy:
		// vlozit na zasobnik pre obsluhu vynimky v uzivatelskom priestore "page fault stack frame" co neviem, co je ale myslim, ze ide o UTrapFrame strukturu
		// spustit obsluhu
		// krok 0 zistim, ci existuje stranka pre exception stack (prostredie si ju podla vsetkych textov ma vytvarat samo)

		// krok 1 zistime si stack pointer
		char* usp = (char*)(tf->tf_esp); // char je 1B a funkcia sizeof vracia velkost v bytoch takze koli smernikovej aritmetike si to chcem ulahcit
	
		// nachadzam sa uz na user exception stacku?
		if((uintptr_t)usp > USTACKTOP) {
			// vnorene volanie mam vynechat jedno slovo (32b) miesta na zasobniku (netusim, ci tam treba nieco zapisat, asi hej ale neviem si spomenut, ze co)
			usp -= 4; // 4*8b = 32b
		}
		else {
			// nenachadzam sa na user exception stacku => treba tam zacat
			usp = (char*)UXSTACKTOP;
		}

		// krok 2 mame pointer na user exception stacku mozeme tam ulozit strukturu UTrapframe
		usp -= sizeof(struct UTrapframe);
		user_mem_assert(curenv, (void*)(usp), sizeof(struct UTrapframe), PTE_W | PTE_U | PTE_P); // pravdepodobne sa niektore prava kontroluju implicitne, all po zbehlej obhliadke kodu si nie som isty (takto to bude fungovat)
		((struct UTrapframe*)usp)->utf_fault_va = fault_va;
		((struct UTrapframe*)usp)->utf_err = tf->tf_err;
		((struct UTrapframe*)usp)->utf_regs = tf->tf_regs;
		((struct UTrapframe*)usp)->utf_eip = tf->tf_eip;
		((struct UTrapframe*)usp)->utf_eflags = tf->tf_eflags;
		((struct UTrapframe*)usp)->utf_esp = tf->tf_esp;

		// krok 3 spustit samotnu obsluhu prerusenia v uzivatelskom priestore
		curenv->env_tf.tf_esp = (uintptr_t)usp; // nastavim zasobnik
		curenv->env_tf.tf_eip = (uintptr_t)curenv->env_pgfault_upcall; // nastavim IP

		env_run(curenv);
	}

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

