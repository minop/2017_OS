// obmedzena implementacia (kopia) IPC pre kernel
#ifndef JOS_KERN_KERNIPC_H
#define JOS_KERN_KERNIPC_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/env.h>
#include <inc/error.h>
#include <inc/memlayout.h>

#include <kern/sched.h>
#include <kern/pmap.h>
#include <kern/env.h>

// kopia z inc/lib.h
// kernipc.c
void	ipc_send(envid_t to_env, uint32_t value, void *pg, int perm);
int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store);
envid_t	ipc_find_env(enum EnvType type);

#endif /* !JOS_KERN_KERNIPC_H */
