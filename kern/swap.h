#ifndef JOS_KERN_SWAP_H
#define JOS_KERN_SWAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/swap.h>

// zmaze PTE_A vsetkym strankam v uzivatelskom priestore vo vsetkych prostrediach
void clear_accessed_flags(void);

#endif /* !JOS_KERN_SWAP_H */
