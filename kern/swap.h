#ifndef JOS_KERN_SWAP_H
#define JOS_KERN_SWAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/swap.h>
#include <inc/env.h>
#include <inc/string.h>

extern struct Mapping **swap_pages;

extern struct Mapping *free_mappings;
extern struct Mapping *mappings;

// nastavenie zretazeneho zoznamu mappingov
void swap_mappings_init(void);

// zmaze PTE_A vsetkym strankam v uzivatelskom priestore vo vsetkych prostrediach
void clear_accessed_flags(void);

// funkcie na zapis/citanie stranok z disku
void swap_page_to_disk(void* stranka, int pozicia, int32_t env_id, struct Mapping *zaznamy);
void swap_page_from_disk(void* stranka, int pozicia, int32_t env_id);

// funkcie obsluhujuce systemove volania
void swap_test(envid_t envid);
void swap_task_done(envid_t evnid);


#endif /* !JOS_KERN_SWAP_H */
