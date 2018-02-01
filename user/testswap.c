// testuje, ci swap stranok na disk funguje spravne

#include <inc/lib.h>

#define ALLOC_PAGE_NUM 32300 // experimentalne urcene, ze je to viac ako volne stranky po spusteni JOSu

void dirtybreakpoint() {
	cprintf("dirty breakpoint\n");
}

void umain(int argc, char **argv) {
	dirtybreakpoint();
	cprintf("Page swapping to disk test\nAllocation starts\n");

	// uistime sa, ze kernel-disk interface bezi
	sys_yield();
	sys_yield();
	sys_yield();
	sys_yield();
	sys_yield();


	int i;
	int* num;
	for(i = 0; i < ALLOC_PAGE_NUM; ++i) {
		cprintf("i = %d; va = %0x\n", i,(USTACKTOP-(i+1)*PGSIZE));
		// alokujem stranku
		sys_page_alloc(thisenv->env_id, (void*)(USTACKTOP-(i+1)*PGSIZE), PTE_P | PTE_U | PTE_W);
		
		// ulozim do nej hodnotu
		num = (int*) (USTACKTOP-(i+1)*PGSIZE);
		*num = i;

		// kontrolne vypisy
		if(i == ALLOC_PAGE_NUM/4) {
			cprintf("25\% done (i = %d)\n", i);
		}
		else if(i == ALLOC_PAGE_NUM/2) {
			cprintf("50\% done (i = %d)\n", i);
		}
		else if(i == ALLOC_PAGE_NUM/4*3) {
			cprintf("75\% done (i = %d)\n", i);
		}
	}

	cprintf("allocation complete\nreading starts\n");
	
	for(i = 0; i < ALLOC_PAGE_NUM; ++i) {
		// nacitam hodnotu a overim
		num = (int*) (USTACKTOP-(i+1)*PGSIZE);
		if(*num != i) {
			panic("page %d did not load properly\n",i);
		}

		// kontrolne vypisy
		if(i == ALLOC_PAGE_NUM/4) {
			cprintf("25\% done (i = %d)\n", i);
		}
		else if(i == ALLOC_PAGE_NUM/2) {
			cprintf("50\% done (i = %d)\n", i);
		}
		else if(i == ALLOC_PAGE_NUM/4*3) {
			cprintf("75\% done (i = %d)\n", i);
		}
	}
}
