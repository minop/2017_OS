// Kernel-level IPC library routines
// upravena kopia z lib/ipc.c nevyuzivajuca systemove volania

#include <kern/kernipc.h>
#include <kern/sched.h>
#include <kern/pmap.h>
#include <kern/env.h>

#include <inc/types.h>
#include <inc/env.h>
#include <inc/error.h>
#include <inc/memlayout.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	int r;

	if (from_env_store)
		*from_env_store = 0;
	if (perm_store)
		*perm_store = 0;

/* som v kernely, takze nemusim vyuzit systemove volanie ale rovno vykonat tie operacie, ktore by vykonalo
	if ((r=sys_ipc_recv(pg ? pg : (void*)UTOP)) < 0)
		return r;
*/
	// podla kern/syscall.c
	// kontrola adresy stranky (nie je zarovnana)
	if(((uintptr_t)pg < UTOP) && ((uintptr_t)pg % PGSIZE))
		return -E_INVAL;
	
	// spustenie komunikacie
	curenv->env_ipc_dstva = pg;
	curenv->env_ipc_recving = true;
	curenv->env_status = ENV_NOT_RUNNABLE;	

	sched_yield(); // pockam na odpoved

	if (from_env_store)
		*from_env_store = curenv->env_ipc_from;
	if (perm_store)
		*perm_store = curenv->env_ipc_perm;
	
	return curenv->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	int r;
	struct Env *env;
	struct PageInfo *pp;
	pte_t *pte;
	
	while (1) {
/* nebudem pouzivat systemove volanie
		r = sys_ipc_try_send(to_env, val, pg ? pg : (void*)UTOP, perm);
		
		if (r == 0)
			break;
		else if (r == -E_IPC_NOT_RECV)
			sys_yield();
		else
			panic("ipc_send: %e", r);
*/
		// podla kern/syscall.c
		// kontrola prijimacieho prostredia
		if((r = envid2env(to_env, &env, 0)) < 0)
			panic("kernipc_send: %e", r);

		if(!env->env_ipc_recving) {
			// -E_IPC_NOT_RECV
			// original implementacia zavolala return, shed_yield a potom sa opakoval cely while cyklus
			// ja teda zavolam shed_yield a potom continue, aby sa docielil rovnaky efekt
			sched_yield();
			continue;
		}
				
		env->env_ipc_perm = 0;
		if(((uintptr_t)pg < UTOP) && ((uintptr_t)env->env_ipc_dstva < UTOP)) {
			if((uintptr_t)pg % PGSIZE) // nie je zarovnana
				panic("kernipc_send: %e", -E_INVAL);
			if((perm & ~PTE_SYSCALL) != 0)
				panic("kernipc_send: %e", -E_INVAL);
			if((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P))
				panic("kernipc_send: %e", -E_INVAL);
			
			pp = page_lookup(curenv->env_pgdir, pg, &pte);
			if(!pp)
				panic("kernipc_send: %e", -E_INVAL);
			if((perm & PTE_W) && !(*pte & PTE_W))
				panic("kernipc_send: %e", -E_INVAL);

			if((r=page_insert(env->env_pgdir, pp, env->env_ipc_dstva, perm)) < 0)
				panic("kernipc_send: %e", r);

			env->env_ipc_perm = perm;
		}

		env->env_ipc_from = curenv->env_id;
		env->env_ipc_value = val;
		env->env_ipc_recving = false;
		env->env_tf.tf_regs.reg_eax = 0;
		env->env_status = ENV_RUNNABLE;

		break; // return 0
	}
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
