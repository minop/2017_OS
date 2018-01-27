// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va; // addr NIE JE PAGE ALIGNED!!!! (vynimka moze nastat hoci kde)
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// (1) islo o chybu vypadku stranky sposobenu zapisom? (error kody v inc/mmu.h)
	if((FEC_WR & err) != FEC_WR) 
		panic("fork pgfault vynimka nebola vyvolana pokusom o zapis, PR: %d WR %d U %d, addr 0x%08x", err&FEC_PR, err&FEC_WR, err&FEC_U, addr);

	// (2) ide o COW stranku?
	if( (uvpd[PDX(addr)] & PTE_P) == 0) /* pritomna? */ 
		panic("fork pgfault stranka nie je mapovana");
	if( (uvpt[PGNUM(addr)] & (PTE_COW | PTE_P)) != (PTE_COW | PTE_P)) /* krava? */ 
		panic("fork pgfault stranka nie je copy-on-write");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	// krok 1 alokovat stranku na PFTEMP
	int perm = ((uvpt[PGNUM(addr)] & PTE_SYSCALL) & ~PTE_COW) | PTE_W; // povolene flagy bez _COW s _W

	r = sys_page_alloc(0, (void*)PFTEMP, perm);
	if(r < 0) 
		panic("fork pgfault chyba pri alokovani docastnej stranky; %e", r);
	
	// krok 2 skopirovat obsah
	memcpy((void*)PFTEMP, (void*)ROUNDDOWN(addr, PGSIZE), PGSIZE); // addr nemusi byt na zaciatku stranky (ten cas, kym som na to prisiel), makro zisti adresu stranky (z inc/mmu.h)

	// krok 3 namapovat docasnu stranku na 'addr'
	r = sys_page_map(0, (void*)PFTEMP, 0, (void*)ROUNDDOWN(addr, PGSIZE), perm); // mapovat musim na page-alligned adresy
	if(r < 0) 
		panic("fork pgfault chyba pri vytvarani mapovania; %e", r);

	// tym, ze som prepisal addr som zrusil povodne mapovanie
	// krok 4 poupratujem po sebe
	r = sys_page_unmap(0, (void*)PFTEMP);
	if(r < 0) 
		panic("fork pgfault chyba pri uvolnovani docasnej stranky; %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	int cow = 0;

	// LAB 4: Your code here.
	int perm = uvpt[PGNUM(pn*PGSIZE)] & PTE_SYSCALL; // len bity povolene pre systemove volania
	
	// pokial nemam stranku zdielat pozriem COW
	if((perm & PTE_SHARE) == 0) {
		if( (perm & (PTE_W | PTE_COW)) != 0) {
			// je nastaveny aspon jeden z priznakov _W _COW (a oni sa vylucuju)
			perm = (perm & ~PTE_W) | PTE_COW; // zmazem _W a pridam COW
			cow = 1;
		}
	}

	// namapujeme stranku pre 'envid' prostredie
	r = sys_page_map(0, (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), perm);
	if(r < 0) 
		panic("fork duppage chyba pri vytvarani mapovania; %e", r);

	// zmenim prava u seba, tak aby som mal priznak _COW pokial som ho pridaval v kopii
	if(cow) {
		r = sys_page_map(0, (void*)(pn*PGSIZE), 0, (void*)(pn*PGSIZE), perm); // prva vec, tkora mi napadla. Je mozne, ze sa to da aj jednoduchsie
		if(r < 0) 
			panic("fork duppage chyba pri zmene prav mapovanim; %e", r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	// krok 1 nastav page fault handler
	set_pgfault_handler(pgfault);  // 'lib/pgfault.c' stara sa aj o vytvorenie zasobnika

	// krok 2 vytvor "dieta" (nove env)
	envid_t dieta = sys_exofork();
	if(dieta < 0) /* chyba */ {
		panic("sys_exofork: %e", dieta);
	}
	else if(dieta == 0) /* dieta */ {
		// podobne ako v 'dumbfork.c' aj tu si musim spravne nastavit premennu thisenv
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// rodic
	// krok 3 chceme skopirovat nase mapovanie do dietata
	int stranka;
	// potrebujem skopirovat od UTEXT po USTACKTOP (normalny zasobnik sa ma kopriovat)
	for(stranka = PGNUM(UTEXT); stranka < PGNUM(USTACKTOP); stranka++) { // duppage robi s scislom stranky
		if( (uvpd[PDX(stranka*PGSIZE)] & (PTE_P | PTE_U)) != (PTE_P | PTE_U))
			continue; // stranka, ktora nie je namapovana je uspesne skopirovana
		if( (uvpt[PGNUM(stranka*PGSIZE)] & (PTE_P | PTE_U)) != (PTE_P | PTE_U)) 
			continue; // pokial stranka neexistuje (obvious) ale aj ked existuje ale uzivatel k nej nema pristup (systemove volanie je naprogramovane tak, ze musi mat) stranku nebudeme kopirovat
		duppage(dieta, stranka);
		// akonahle sa skopiruje mapovanie poslednej stranky (resp. stranky na zasobniku, ktora sa aktualne pouziva) nastane vypadok stranky (bodaj by aj nenastal, kedze prostredie chce volat funkcie a pracovat so svojim zasobnikom)
		// tento vypadok sa teda hned osetri a tato "najspodnejsia" stranka zasobniku sa naozaj skopiruje
	}

	// krok 4 nastavit page fault handler pre dieta (podla obhliadky kodu by sa nemal skopirovat pri exoforku a dokonca sa explicitne resetuje v env_alloc)
	// nemozem pouzit set_pgfault_handler funkciu, pretoze tu musi zavolat prostredie samo a akonahle by chcelo volat funkciu nastal by vypadok (lebo zasobnik je COW) a nemalo by ho co osetrit
	r = sys_env_set_pgfault_upcall(dieta, thisenv->env_pgfault_upcall);
	if(r < 0) 
		panic("fork chyba pri nastavovani funkcie na obsluhu vypadku stranky pre detske prostredie; %e", r);

	// namapovanie zasobnika pre obsluhu
	r = sys_page_alloc(dieta, (void*)(UXSTACKTOP-PGSIZE), PTE_P | PTE_U | PTE_W);	
	if(r < 0) 
		panic("fork chyba pri mapovani stranky zasobnika pre detske prostredie; %e", r);
	// funkcia 'set_pgfault_handler' toho robila trosku viac, takze uvidime, ako toto bude fungovat

	// krok 5 nastavit dieta ako RUNNABLE
	r = sys_env_set_status(dieta, ENV_RUNNABLE);
	if(r < 0) 
		panic("fork chyba pri zmene statusu dietata na runnable; %e", r);
	return dieta;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
