/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "pmm.h"
#include "vmm.h"
#include "spike_interface/spike_utils.h"
#include "kernel/sync_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {

  int hartid = read_tp();
  process* proc = current[hartid];

  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert(proc);
  char* pa = (char*)user_va_to_pa((pagetable_t)(proc->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {

  int hartid = read_tp();

  sprint("hartid = %d: User exit with code:%d.\n", hartid, code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  static volatile int exit_counter = 0;
  sync_barrier(&exit_counter, NCPU);

  if(hartid == 0) {
    sprint("hartid = %d: shutdown with code:%d.\n", hartid, code);
    shutdown(code);
  }

  return 0;
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page() {

  int hartid = read_tp();
  process *proc = current[hartid];

  void* pa = alloc_page();
  uint64 va = g_ufree_page[hartid];
  g_ufree_page[hartid] += PGSIZE;
  user_vm_map((pagetable_t)proc->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));
  sprint("hartid = %d: vaddr 0x%x is mapped to paddr 0x%x\n", hartid, va, pa);
  return va;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {

  int hartid = read_tp();
  process *proc = current[hartid];

  user_vm_unmap((pagetable_t)proc->pagetable, va, PGSIZE, 1);
  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    // added @lab2_2
    case SYS_user_allocate_page:
      return sys_user_allocate_page();
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
