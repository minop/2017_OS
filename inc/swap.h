// hlavickovy subor pre swapovanie stranok na disk
#ifndef JOS_INC_SWAP_H
#define JOS_INC_SWAP_H

#include <inc/fs.h>
#include <inc/mmu.h>
#include <inc/types.h>
#include <inc/memlayout.h>

// Maximalny pocet stranok, ktore mozem mat naraz na disku (zmestia sa do jedneho suboru)
#define MAXSWAPPEDPAGES	MAXFILESIZE/PGSIZE

// Flag na oznacenie, ze stranka sa nachadza na disku
#define PTE_SWAP	0x200	// mal by to byt posledny volny flag, lebo 0x800 je PTE_COW a 0x400 je PTE_SHARE

// urcuje kazde kolke prerusenie casovaca sa zmazu PTE_A bity
#define MAXPERIODA 64

//struktura ktora bude udrziavat informacie o tom, ktore prostredie ma ktoru virtualnu adresu namapovanu na fyzicku stranku
struct Mapping{
	int32_t env_id;		//prostredie, ktore mapuje na fyzicku stranku
	uintptr_t va;		//virtualna adresa, na ktoru sa mapuje fyzicka stranka
	struct Mapping *next;	//dalsie mapovanie na fyzicku stranku v zretazenom zozname
};

// adresy stranok pre komunikaciu s rozhranim
#define SWAPREQ		0xd0000000
#define SWAPPAGE	SWAPREQ + PGSIZE

// typy poziadavok
enum {
	SWAP_INIT = 0,
	SWAP_READ,
	SWAP_WRITE
};

// struktura na komunikaciu s pomocnym prostredim
struct Poziadavka{
	unsigned typ;
	uint32_t pozicia;
	pde_t *pgdir;		// len pre citanie
	uintptr_t adresa;	// len pre citanie
	uint32_t env_id;
};
#endif /* !JOS_INC_SWAP_H */
