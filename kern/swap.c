#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/env.h>
#include <inc/types.h>
#include <inc/string.h>

#include <kern/swap.h>
#include <kern/env.h>
#include <kern/pmap.h>

//vytvaram pole struktur Mapping, pole zretazenych zoznamov predstavujucich mapovanie na fyzicku stranku. Kazdy prvok pola predstavuje fyzicku stranku. Pole ma teda velkost MAXSWAPPEDPAGES.
struct Mapping *swap_pages = NULL;

// pomocna struktura pri vyhadzovani stranky
struct Kandidat {
	PageInfo pi* = NULL;
	int trieda = 4;
};

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
		
		// stranky budem vyhadzovat len obycajnym prostrediam
		if(e->env_type == ENV_TYPE_USER) {
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
	}
		
	return;
}

// funkcia, ktora vyberie stranku na vyhodenie na disk a potom restartuje prostredie, ktore ju spustilo
void swap_evict_page() {
	// potrebujem najst kandidata na vyhodenie
	// algoritmus je NRU - Not Recently Used (podla wikipedie)
	// stranky su rozdelene do 4 kategorii podla stavov ich Accessed a Dirty bitov
	// (podla wikipedie: Page_replacement_algorithm)
	// 3. referenced, modified 		PTE_A + PTE_D
	// 2. referenced, not modified		PTE_A   
	// 1. not referenced, modified		        PTE_D
	// 0. not referended, not modified
	
	// musim prejst postupne vsetky stranky vo vsetkych prostrediach a najst co najlepsieho kandidata (ked najdem 0 tak skoncim)
	// nasledne musim najst vsetky prostredia, ktore k danej stranke pristupuju, pridat ich do struktury a oznacit stranky bitom PTE_SWAP
	// potom musim obsah stranky zapisat na disk na prislusnej pozicii (podla struktury)
	// nakoniec (uz v funkcii obsluhujucej systemove volanie) musim dotknutu stranku uvolnit a spustit prostredie, ktore zavolalo tuto funkciu

	// prejdem vsetky prostredia a pozriem bity
	// kod na prechadzanie prostredi je podla toho na mazanie bitov
	struct Env *aktualne;	// pointer na prve prostredie
	int i;
	pte_t *zaznam;		// na kontrolu
	uintptr_t va;		// virtualna adresa
	struct PageInfo *pi;
	struct Kandidat kand;
	bool najdene = false;

	for(i = 0; (i < NENV) || !najdene; ++i) {
		aktualne = &envs[i]; // i-te prostredie
		
		// prezeram len obycajne prostredia (iba tam som mazal bity)
		if(e->env_type == ENV_TYPE_USER) {
			// je prostredie RUNNING alebo RUNNABLE?
			if(e->env_status == ENV_RUNNABLE || e->env_status == ENV_RUNNING) {
			
				// postupne vsetky stranky od UTEXT po UTOP
				for(va = UTEXT; va < UTOP; va+=PGSIZE) {
				
					// ziskam si zaznam
					pi = page_lookup(e->env_pgdir, (void*)va, &zaznam);

					if(pi != NULL) {
						// nachadza sa tu stranka => vyhodnotim jej vhodnost
						int trieda = ((*zaznam & PTE_A)/PTE_A)*2 + ((*zaznam & PTE_D)/PTE_D);
						
						// je to 0?
						if(trieda == 0) {
							// nasiel som svoju stranku
							kand.pi = pi;
							najdene = true; // na vyjdenie z vonkajsieho cyklu
							break;
						}
						// je to menej ako aktualny kandidat?
						else if(kand.trieda < trieda){
							kand.trieda = trieda;
							kand.pi = pi;
						}
					}
				}	
			}
		}
	}

	// v kand.pi je pointer na PageInfo stranky, 
	
}


// pomocna funkcia, na ziskanie Env pointra na komunikacne prostredie
struct Env* getInterfacePointer() {
	int i;
	for(i = 0; i < NENV; ++i) {
		if(envs[i].env_type == ENV_TYPE_SWAP) {
			return &envs[i];
		}
	}
	
	// ak som sa dostal az sem tak som prostredie nenasiel
	return NULL;
}

// funkcie zabezpecujuce zapisovanie a citanie stranok z/na disk

// ulozi stranku zo vstupnej adresy na n-tu poziciu na disk
// funkcia sa vracia, pokial je prostredie v inom stave ako NOT_RUNNABLE
// funkcia sa nevracia ked vykona poziadavku (lebo spusta prostredie)
// po vykonani poziadavky sa pokracuje vo funkcii 'swap_task_done'
// ked nastane chyba (napr. adresa nie je zarovnana na stranky) vola sa panic
void swap_page_to_disk(void* stranka, int pozicia) {
	// overim adresu
	if(((uintptr_t)stranka % PGSIZE) != 0)
		panic("swap_page_to_disk: adresa nie je zarovnana na stranky");

	// zistim, ci existuje prostredie a ci je v stave NOT_RUNNABLE
	struct Env* e = getInterfacePointer();
	if(e == NULL)
		panic("swap_page_to_disk: nemam interface prostredie");
	if(e->env_status != ENV_NOT_RUNNABLE)
		return;

	// nakopirujem si obsah stranky do komunikacneho prostredia
	// krok 1: zistim ktoru stranku prostredie pouziva ako buffer
	struct PageInfo *pi = page_lookup(e->env_pgdir, (void*)SWAPPAGE, NULL);
	
	if(pi == NULL)
		panic("swap_page_to_disk: stratil pomocnu stranku");
	
	// krok 2: nakopirujem data z vstupnej stranky do prostredia
	memcpy((void*)page2kva(pi), stranka, PGSIZE);

	// prostredie ma data, ktore musi zapisat => nastavim mu ulohu
	// opat si musim ziskat pointer na jeho stranku
	pi = page_lookup(e->env_pgdir, (void*)SWAPREQ, NULL);
	
	if(pi == NULL)
		panic("swap_page_to_disk: stratil request stranku");

	struct Poziadavka *p = (struct Poziadavka*)page2kva(pi);
	p->typ = SWAP_WRITE;
	p->pozicia = pozicia;

	// prostredie ma potrebne data AJ prikaz spustim ho (a pre istotu ho predtym nastavim na runnable)
	e->env_status = ENV_RUNNABLE;
	env_run(e); // koniec funkcie
}

// ulozi obsah n-tej pozicie na disku do zadanej adresy
// funkcia sa vracia, pokial je prostredie v inom stave ako NOT_RUNNABLE
// funkcia sa nevracia ked vykona poziadavku (lebo spusta prostredie)
// po vykonani poziadavky sa pokracuje vo funkcii 'swap_task_done'
// ked nastane chyba (napr. adresa nie je zarovnana na stranky) vola sa panic
void swap_page_from_disk(void* stranka, int pozicia) {
	// overim adresu
	if(((uintptr_t)stranka % PGSIZE) != 0)
		panic("swap_page_from_disk: adresa nie je zarovnana na stranky");

	// zistim, ci existuje prostredie a ci je v stave NOT_RUNNABLE
	struct Env* e = getInterfacePointer();
	if(e == NULL)
		panic("swap_page_from_disk: nemam interface prostredie");
	if(e->env_status != ENV_NOT_RUNNABLE)
		return;

	// trochu zlozitejsie, najprv musim zavolat request az potom mozem presuvat data zo stranok (lebo su najprv na disku)
	// spracovanie (presun dat) prebieha v systemovom volani
	struct PageInfo *pi = page_lookup(e->env_pgdir, (void*)SWAPREQ, NULL);
	
	if(pi == NULL)
		panic("swap_page_from_disk: stratil request stranku");

	struct Poziadavka *p = (struct Poziadavka*)page2kva(pi);
	p->typ = SWAP_READ;
	p->pozicia = pozicia;
	p->adresa = (uintptr_t)stranka;
	p->pgdir = curenv->env_pgdir; // ked sa vynorim tak som v inom prostredi, potrebujem teda vediet kam mam nahrat data

	// prostredie ma potrebne data AJ prikaz spustim ho (a pre istotu ho predtym nastavim na runnable)
	e->env_status = ENV_RUNNABLE;
	env_run(e); // koniec funkcie
}

// funkcie na obsluhu systemovych volani
void swap_test(envid_t envid) {
	
	int r;
	struct Env *e;

	// skontrolujem ci prostredie je typu SWAP
	if((r = envid2env(envid, &e, 0)) < 0)
		return; // volanie zavolane s nahodnym parametrom
	if(e->env_type != ENV_TYPE_SWAP)
		return; // prostredie, nie je moj kamos
	
	// nastavim prostredie na nie RUNNABLE (caka na dalsiu ulohu)
	e->env_status = ENV_NOT_RUNNABLE;

	// TODO testy (patrilo, by sa)
	cprintf("hello from syscall in kernel\n");
}

void swap_task_done(envid_t envid) {
	
	int r;
	struct Env *e;

	// skontrolujem ci prostredie je typu SWAP
	if((r = envid2env(envid, &e, 0)) < 0)
		return; // volanie zavolane s nahodnym parametrom
	if(e->env_type != ENV_TYPE_SWAP)
		return; // prostredie, nie je moj kamos

	// nastavim prostredie na nie RUNNABLE (caka na dalsiu ulohu)
	e->env_status = ENV_NOT_RUNNABLE;

	// ziskam si stranku s requestom
	struct PageInfo *pi = page_lookup(e->env_pgdir, (void*)SWAPREQ, NULL);
	
	if(pi == NULL)
		panic("swap_task_done: stratil request stranku");
	
	// zistim, ci som obsluhoval citanie alebo zapis
	struct Poziadavka *p = (struct Poziadavka*)page2kva(pi);
	switch(p->typ) {
		case SWAP_READ:
			// cital som, takze musim vytiahnut data z prostredia na pozadovanu adresu
			// adresu tu nemam ale ulozil som si ju do requestu

			// ziskam si stranku s datami z prostredia
			pi = page_lookup(e->env_pgdir, (void*)SWAPPAGE, NULL);
	
			if(pi == NULL)
				panic("swap_task_done: stratil pomocnu stranku");

			// ziskam cielovu stranku (lebo som zmenil kontext)
			// potom to znamena, ze netreba oklukov ziskat tie predchadzajuce....
			struct PageInfo *dpi = page_lookup(p->pgdir, (void*)p->adresa, NULL);
			if(dpi == NULL)
				panic("swap_task_done: stranka, kam som mal zapisat zmyzla");

			// skopirujem
			memcpy((void*)p->adresa, (void*)page2kva(dpi), PGSIZE);

			// TODO dalsie spracovanie po nacitani
			break;
	}	
}
