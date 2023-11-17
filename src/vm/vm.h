#ifndef VM_VM_H
#define VM_VM_H

#include "filesys/off_t.h"
#include <hash.h>
#include <list.h>

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry{
  bool writable;
  int type;
  void *vaddr;
  struct hash_elem elem;

  struct thread* thread;
  struct file* file;
  off_t file_offset;
  off_t file_bytes;

  struct list_elem mmap_elem;
  size_t used_index;
  bool is_loaded;


};


void vm_destory(struct hash *vm);
void vm_init(struct hash* vm);
struct vm_entry* find_vme(void *addr);
bool insert_vme(struct hash* vm, struct vm_entry *vme);
bool delete_vme(struct hash* vm, struct vm_entry *vme);
struct vm_entry* vm_entry_alloc(void* addr);

#endif /* vm/vm.h */
