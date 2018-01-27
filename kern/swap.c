#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/env.h>
#include <inc/types.h>

#include <kern/swap.h>
#include <kern/env.h>
#include <kern/pmap.h>

//vytvaram pole struktur Mapping, pole zretazenych zoznamov predstavujucich mapovanie na fyzicku stranku. Kazdy prvok pola predstavuje fyzicku stranku. Pole ma teda velkost MAXSWAPPEDPAGES.
struct Mapping *swap_pages = NULL;



// zmaze PTE_A bit na vsetkych strankach [UTEXT,UTOP), aby sa dal pouzit NRU (Not Recently Used)
// algoritmus na vyber stranky na swap
void clear_accessed_flags() {
	// potrebujem prejst vsetky prostredia, ktore su RUNNING alebo RUNNABLE a zmazat im PTE_A flag na vsetkych strankach, ktore su medzi UTEXT a UTOP (swapovat na disk budem len tieto stranky)

	struct Env *e; // pointer na prve prostredie
	int i;
	pte_t *zaznam; // na zmenenie flagov
	uintptr_t va; // virtualna adresa
	struct PageInfo *pi;

	for(i = 0; i < NENV; ++i) {
		e = &envs[i]; // i-te prostredie

		// je prostredie RUNNING alebo RUNNABLE?
		if(e->env_status == ENV_RUNNABLE || e->env_status == ENV_RUNNING) {
			
			// postupne vsetky stranky od UTEXT po UTOP
			for(va = UTEXT; va < UTOP; va+=PGSIZE) {
				
				// ziskam si zaznam
				pi = page_lookup(e->env_pgdir, (void*)va, &zaznam);

				if(pi != NULL) {
					// nejaka stranka sa na adrese 'va' nachadza => zmazem PTE_A bit
					*zaznam &= ~PTE_A;
				}
			}	
		}
	}
		
	return;
}
