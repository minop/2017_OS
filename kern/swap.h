#ifndef JOS_KERN_SWAP_H
#define JOS_KERN_SWAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/swap.h>

extern struct Mapping *swap_pages;

#endif /* !JOS_KERN_SWAP_H */
