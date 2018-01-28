#include <inc/lib.h>
#include <inc/swap.h>

void umain(int args, char **argv) {
	
	// prikazy z kernelu si precitame zo struktury na adrese SWAPREQ, vykoname ich a pomocou systemoveho volania oznamime kernelu splnenie ulohy

	int fd;
	char *stranka = (char*)SWAPPAGE;
	struct Poziadavka *req = (struct Poziadavka *)SWAPREQ;
	int r, n;

	// ked sa spustim musim si otvorit subor na swapovanie

	if((fd = open("/swap", O_RDWR | O_CREAT)) < 0)
		panic("kernel-disk interface nevie otvorit/vytvorit swapovaci subor; %e", fd);

	// alokujem si svoje dve potrebne stranky
	if((r = sys_page_alloc(thisenv->env_id, (void*)SWAPREQ, PTE_U | PTE_W | PTE_P)) < 0)
		panic("sys_page_alloc: %e\n", r);
	if((r = sys_page_alloc(thisenv->env_id, (void*)SWAPPAGE, PTE_U | PTE_W | PTE_P)) < 0)
		panic("sys_page_alloc: %e\n", r);

	cprintf("Kernel-disk interface running\n");

	// oznami kernelu, ze kernel-disk interface bezi a necha ho urobit testy
	sys_swap_test(thisenv->env_id);

	while(1) {
		if(req->typ != SWAP_INIT) {
			// nastavim sa na spravnu poziciu
			if((r = seek(fd, req->pozicia*PGSIZE)) < 0)
				panic("kernel-disk interface nastavenie na poziciu zlyhalo; %e", r);

			// precitam si poziadavku z adresy pre poziadavky
			switch(req->typ) {
				case SWAP_READ:
					if((r = readn(fd, (void*)stranka, PGSIZE)) < 0)
						panic("readn: %e", r);
					
					if(r < PGSIZE)
						panic("nenacital som celu stranku z disku!");
				
					sys_swap_task_done(thisenv->env_id);
					break;

				case SWAP_WRITE:
					// write moze zapisat aj menej ako n bytov a nie je implementovana funkcia, co zaruci n => musim si spravit sam
					n = 0;
					while(n != PGSIZE) {
						if((r = write(fd, (void*)(stranka+n), PGSIZE-n)) < 0)
							panic("write: %e", r);
					
						n += r; // pripocitam pocet zapisanych bytov
					}

					sys_swap_task_done(thisenv->env_id);
					break;
			}
		}
		else {
			// ak vsetko funguje, tak by som sa sem nidky nemal normalne dostat, ale pre istotu...
			sys_yield();
		}
	}
}
