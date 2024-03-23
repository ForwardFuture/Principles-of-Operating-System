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

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page(void) {
  void* pa = alloc_page();
  uint64 va = g_ufree_page;
  g_ufree_page += PGSIZE;
  user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));

  return va;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {
  user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  return 0;
}

//
// free block
//
uint64 sys_user_free_block(uint64 va) {

  block *tag_block = current->used_block_header, *pre_block = NULL;

  while(tag_block != NULL) {
    if(tag_block->va_addr <= va && va <= tag_block->va_addr + tag_block->block_size)break;
    pre_block = tag_block;
    tag_block = tag_block->nxt;
  }

  if(tag_block == NULL) return -1;

  if(pre_block != NULL) {
    pre_block->nxt = tag_block->nxt;
  }
  else current->used_block_header = tag_block->nxt;

  block *free_block = current->free_block_header;
  pre_block = NULL;

  while(free_block != NULL) {
    if(free_block->va_addr > va)break;
    pre_block = free_block;
    free_block = free_block->nxt;
  }

  if(free_block != NULL) {
    if(ROUNDUP(tag_block->va_addr + tag_block->block_size, 8) == free_block->va_addr - sizeof(block)) {
      tag_block->block_size += free_block->block_size + sizeof(block);
      tag_block->nxt = free_block->nxt;

      int k = 1;
      while(ROUNDUP(tag_block->va_addr, PGSIZE) + k * PGSIZE <= tag_block->va_addr + tag_block->block_size)k++;
      k--;
      if(k) {
        tag_block->block_size -= k * PGSIZE;
        uint64 st_va = ROUNDUP(tag_block->va_addr, PGSIZE);
        for(int i = 0; i < k; i++)
          sys_user_free_page(st_va + i * PGSIZE);
      }

    }
    else tag_block->nxt = free_block->nxt;

  }

  if(pre_block != NULL) {
    if(ROUNDUP(pre_block->va_addr + pre_block->block_size, 8) == tag_block->va_addr - sizeof(block)) {
      pre_block->block_size += tag_block->block_size + sizeof(block);
      pre_block->nxt = tag_block->nxt;

      int k = 1;
      while(ROUNDUP(pre_block->va_addr, PGSIZE) + k * PGSIZE <= pre_block->va_addr + pre_block->block_size)k++;
      k--;
      if(k) {
        pre_block->block_size -= k * PGSIZE;
        uint64 st_va = ROUNDUP(pre_block->va_addr, PGSIZE);
        for(int i = 0; i < k; i++)
          sys_user_free_page(st_va + i * PGSIZE);
      }
    }
    else pre_block->nxt = tag_block->nxt;
  }
  else current->free_block_header = tag_block;

  return 0;
}

//
// allocate block
//
uint64 sys_user_allocate_block(int n) {

  //sprint("*************\n");

  int alloc_memory = ROUNDUP(n, 8);
  //sprint("%d\n", alloc_memory);
  block* available_block = current->free_block_header;
  block* pre_block = NULL;

  //sprint("%lx\n", available_block);

  //sprint("*************\n");

  while(available_block != NULL) {
    if(available_block->block_size >= alloc_memory)break;
    pre_block = available_block;
    available_block = available_block->nxt;
  }

  //sprint("%lx\n", available_block);

  // Find no blocks that we expect, allocate a new page
  if(available_block == NULL) {

    // allocate one page
    void* pa = alloc_page();
    uint64 va = g_ufree_page;
    block *new_block = (block *)pa;

    new_block->nxt = NULL;
    new_block->va_addr = va + sizeof(block);
    new_block->pa_addr = (uint64)pa + sizeof(block);
    new_block->block_size = PGSIZE - sizeof(block);
    g_ufree_page += PGSIZE;
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
          prot_to_type(PROT_WRITE | PROT_READ, 1));

    // allocate more pages
    while(new_block->block_size < alloc_memory) {
      pa = alloc_page();
      va = g_ufree_page;
      new_block->block_size += PGSIZE;
      g_ufree_page += PGSIZE;
      user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
            prot_to_type(PROT_WRITE | PROT_READ, 1));
    }
    
    if(pre_block != NULL) {
      if(ROUNDUP(pre_block->va_addr + pre_block->block_size, 8) == new_block->va_addr - sizeof(block)) {
        pre_block->block_size += new_block->block_size + sizeof(block);
        available_block = pre_block;
      }
      else {
        pre_block->nxt = new_block;
        available_block = new_block;
      }
    }
    else {
      current->free_block_header = new_block;
      available_block = new_block;
    }

  }

  // till now, we've found the available_block

  return alloc_block(available_block, pre_block, alloc_memory);

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
      return sys_user_allocate_block(a1);
    case SYS_user_free_page:
      return sys_user_free_block(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
