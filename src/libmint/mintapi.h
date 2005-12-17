#if !(defined MINTAPI_H)
#define MINTAPI_H

#include "Snippets.h"
#include "icode.h"

// Returns the number of CPUs the system has
int rsesc_get_num_cpus(void);
ThreadContext*rsesc_get_thread_context(Pid_t pid);

void rsesc_spawn(Pid_t ppid, Pid_t cpid, int flags);

int  rsesc_exit(int pid, int err);
void rsesc_simulation_mark(int pid);
void rsesc_simulation_mark_id(int pid,int id);
void rsesc_fast_sim_begin(int pid);
void rsesc_fast_sim_end(int pid);
int  rsesc_suspend(int pid, int tid);
int  rsesc_resume(int pid, int tid);
int  rsesc_yield(int pid, int tid);
void rsesc_preevent(int pid, int vaddr, int type, void *sptr);
void rsesc_postevent(int pid, int vaddr, int type, void *sptr);
void rsesc_memfence(int pid, int vaddr);
void rsesc_acquire(int pid , int vaddr);
void rsesc_release(int pid , int vaddr);

#if (defined TLS)
icode_ptr rsesc_get_instruction_pointer(int pid);
void rsesc_set_instruction_pointer(int pid, icode_ptr picode);
int rsesc_replay_system_call(int pid);
void rsesc_record_system_call(int pid, unsigned int retVal);

void rsesc_begin_epochs(int pid);
void rsesc_future_epoch(int pid);
void rsesc_future_epoch_jump(int pid, icode_ptr jump_icode);
void rsesc_acquire_begin(int pid);
void rsesc_acquire_retry(int pid);
void rsesc_acquire_end(int pid);
void rsesc_release_begin(int pid);
void rsesc_release_end(int pid);
void rsesc_commit_epoch(int pid);
void rsesc_change_epoch(int pid);
void rsesc_end_epochs(int pid);

int  rsesc_syscall_replay(int pid);
void rsesc_syscall_new(int pid);
void rsesc_syscall_retval(int pid, int value);
void rsesc_syscall_errno(int pid, int value);
void rsesc_syscall_perform(int pid);
#endif

#ifdef TASKSCALAR
void rsesc_exception(int pid);
void rsesc_spawn(int ppid, int pid, int flags);
int  rsesc_exit(int cpid, int err);
int  rsesc_version_can_exit(int pid);
void rsesc_prof_commit(int pid, int tid);
void rsesc_fork_successor(int ppid, int where, int tid);
int  rsesc_become_safe(int pid);
int  rsesc_is_safe(int pid);
int rsesc_OS_prewrite(int pid, int addr, int iAddr, int flags);
void rsesc_OS_postwrite(int pid, int addr, int iAddr, int flags);
int rsesc_OS_read(int pid, int addr, int iAddr, int flags);
int  rsesc_is_versioned(int pid);
#endif

#if (defined TASKSCALAR) || (defined TLS)
int rsesc_OS_read(int pid, int addr, int iAddr, int flags);
void rsesc_OS_read_string(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size);
void rsesc_OS_read_block (int pid, int iAddr, void *dstStart, const void *srcStart, size_t size);
void rsesc_OS_write_block(int pid, int iAddr, void *dstStart, const void *srcStart, size_t size);
#endif

icode_ptr mint_exit(icode_ptr picode, thread_ptr pthread);

// Functions exported by mint for use in sesc

int mint_sesc_create_clone(thread_ptr pthread);
void mint_sesc_die(thread_ptr pthread);

#endif /* !(defined MINTAPI_H) */
