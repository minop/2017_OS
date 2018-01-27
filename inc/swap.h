// hlavickovy subor pre swapovanie stranok na disk
#ifndef JOS_INC_SWAP_H
#define JOS_INC_SWAP_H

#include <inc/fs.h>
#include <inc/mmu.h>

// Maximalny pocet stranok, ktore mozem mat naraz na disku (zmestia sa do jedneho suboru)
#define MAXSWAPEDPAGES	MAXFILESIZE/PGSIZE

// Flag na oznacenie, ze stranka sa nachadza na disku
#define PTE_SWAP	0x200	// mal by to byt posledny volny flag, lebo 0x800 je PTE_COW a 0x400 je PTE_SHARE

// urcuje kazde kolke prerusenie casovaca sa zmazu PTE_A bity
#define MAXPERIODA 64

#endif /* !JOS_INC_SWAP_H */
