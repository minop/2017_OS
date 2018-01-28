#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/env.h>
#include <inc/types.h>
#include <inc/string.h>

#include <kern/swap.h>
#include <kern/env.h>
#include <kern/pmap.h>

//vytvaram pole struktur Mapping, pole zretazenych zoznamov predstavujucich mapovanie na fyzicku stranku. Kazdy prvok pola predstavuje fyzicku stranku. Pole ma teda velkost MAXSWAPPEDPAGES.
struct Mapping **swap_pages = NULL;

struct Mapping *mappings = NULL;
struct Mapping *free_mappings = NULL;

// pomocna struktura pri vyhadzovani stranky
struct Kandidat {
	struct PageInfo *pi;
	int trieda;
};

// spravne nastavy zretazeny zoznam volnych mapovacich struktur
void swap_mappings_init() {
	int i;
	for(i = 0; i < MAXMAPPINGS; ++i) {
		mappings[i].next = free_mappings;
		free_mappings = &mappings[i];
	}
}

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
	int i, zi;
	pte_t *zaznam;		// na kontrolu
	uintptr_t va;		// virtualna adresa
	struct PageInfo *pi;
	struct Kandidat kand;
	bool najdene = false;

	kand.trieda = 4;

	for(i = 0; (i < NENV) || !najdene; ++i) {
		aktualne = &envs[i]; // i-te prostredie
		
		// prezeram len obycajne prostredia (iba tam som mazal bity)
		if(aktualne->env_type == ENV_TYPE_USER) {
			// je prostredie RUNNING alebo RUNNABLE?
			if(aktualne->env_status == ENV_RUNNABLE || aktualne->env_status == ENV_RUNNING) {
			
				// postupne vsetky stranky od UTEXT po UTOP
				for(va = UTEXT; va < UTOP; va+=PGSIZE) {
				
					// ziskam si zaznam
					pi = page_lookup(aktualne->env_pgdir, (void*)va, &zaznam);

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

	// pred tym,i nez najdem vsetky vyskyty stranky, si najdem volne miesto v strukture mapujucej stranky na disku na prostredia, a ak tam nie je skoncim
	struct Mapping **zoznam = NULL;
	for(zi = 0; zi < MAXSWAPPEDPAGES; zi++) {
		if(swap_pages[zi] == NULL) {
			// volny zoznam
			zoznam = &swap_pages[zi];
			break;
		}
	}
	if(zi == MAXSWAPPEDPAGES)
		panic("swap_evict_page: nemam miesto na disku na vyhodenie dalsej stranky\n");

	// v kand.pi je pointer na PageInfo najlepsej stranky
	// potrebujem prejst vsetky prostredia a vsetky stranky (znovu) a vsetky vyskyty pridat do struktury ak je este miesto
	// (pomocne premenne na prechadzanie cyklom mozem zrecyklovat)
	for(i = 0; i < NENV; ++i) {
		aktualne = &envs[i]; // i-te prostredie
		
		// prezeram len obycajne prostredia (iba tam som mazal bity)
		if(aktualne->env_type == ENV_TYPE_USER) {
			// je prostredie RUNNING alebo RUNNABLE?
			if(aktualne->env_status == ENV_RUNNABLE || aktualne->env_status == ENV_RUNNING) {
			
				// postupne vsetky stranky od UTEXT po UTOP
				for(va = UTEXT; va < UTOP; va+=PGSIZE) {
				
					// ziskam si zaznam
					pi = page_lookup(aktualne->env_pgdir, (void*)va, &zaznam);

					// moja stranka?
					if(pi == kand.pi) {
						*zaznam |= PTE_SWAP; // stranka je na disku
						
						// su volne mappingy?
						if(free_mappings == NULL)
							panic("swap_evict_page: nemam volny mapping na vyhodenie stranky\n");

						// vyberiem si novy mapping zo zoznamu volnych
						struct Mapping *novy = free_mappings;
						free_mappings = free_mappings->next;

						// nastavim mu hodnoty
						novy->env_id = aktualne->env_id;
						novy->va = va;

						// pridam do zoznamu
						novy->next = *zoznam;
						*zoznam = novy;
					}
				}	
			}
		}
	}
 
	// ak som sa dostal az sem bez paniky, tak vsetky prostredia, ktore sa na stranku odkazovali su v zozname, mozem dat stranku zapisat
	swap_page_to_disk(page2kva(kand.pi), zi, curenv->env_id, *zoznam);
	// sem sa dostanem iba, ak funkcia vratila - interface je v nejakom random stave
	panic("swap_evict_page: kern-disk interface nie je v stave NOT RUNNABLE");
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

// pomocna funkcia, ktora "jemne" odmapuje vsetky stranky zo zoznamu
// ~kopia funkcie page_remove z kern/pmap.c
void soft_page_remove_all(struct Mapping *zoznam) {
	while(zoznam != NULL) {
		struct Env *e;
		pte_t *pte;
		int r;
		
		// ziskam si prostredie
		if((r = envid2env(zoznam->env_id, &e, 0)) == 0) {
			// prostredie je legit
			
			// ziskam si zaznam
			page_lookup(e->env_pgdir, (void*)zoznam->va, &pte);

			if(pte != NULL) {
				// naozaj tam je stranka
				if(*pte & PTE_P) {
					struct PageInfo *pi = pa2page(*pte);
					
					page_decref(pi);

					// nastavim len PTE_P bit na 0, aby sa dala stranka obnovit s povodnymi pravami
					*pte &= ~PTE_P;
					
					tlb_invalidate(e->env_pgdir, (void*)zoznam->va);
				}
			}
		}

		zoznam = zoznam->next;	
	}
}

// funkcie zabezpecujuce zapisovanie a citanie stranok z/na disk

// ulozi stranku zo vstupnej adresy na n-tu poziciu na disk
// funkcia sa vracia, pokial je prostredie v inom stave ako NOT_RUNNABLE
// funkcia sa nevracia ked vykona poziadavku (lebo spusta prostredie)
// po vykonani poziadavky sa pokracuje vo funkcii 'swap_task_done'
// ked nastane chyba (napr. adresa nie je zarovnana na stranky) vola sa panic
void swap_page_to_disk(void* stranka, int pozicia, int32_t env_id, struct Mapping *zaznamy) {
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
	p->adresa = (uintptr_t)stranka;
	p->env_id = env_id;
	p->zaznamy = zaznamy;

	// prostredie ma potrebne data AJ prikaz spustim ho (a pre istotu ho predtym nastavim na runnable)
	e->env_status = ENV_RUNNABLE;
	env_run(e); // koniec funkcie
}

// ulozi obsah n-tej pozicie na disku do zadanej adresy
// funkcia sa vracia, pokial je prostredie v inom stave ako NOT_RUNNABLE
// funkcia sa nevracia ked vykona poziadavku (lebo spusta prostredie)
// po vykonani poziadavky sa pokracuje vo funkcii 'swap_task_done'
// ked nastane chyba (napr. adresa nie je zarovnana na stranky) vola sa panic
void swap_page_from_disk(void* stranka, int pozicia, int32_t env_id) {
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
	p->env_id = env_id;

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
		case SWAP_WRITE:
			// obsah stranky som zapisal na disk, mozem uvolnit stranky
			soft_page_remove_all(p->zaznamy);

			// TODO dalsie spracovanie po zapisani
			break;
	}

	// spustim prostredie z ktoreho som volal swapovacie funkcie	
	if(envid2env(p->env_id, &e, 0) < 0)
		panic("swap_task_done: neviem sa vratit k povodnemu prostrediu\n");
	
	//prostredie znovu spustim
	env_run(e);
}
