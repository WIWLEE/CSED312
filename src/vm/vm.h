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

  struct thread* thread;
  struct file* file;
  off_t file_offset;
  off_t file_bytes;

  size_t used_index;

  struct hash_elem elem;
  struct list_elem mmap_elem;
  bool is_loaded;
};

// file mapping을 위한 structure
struct mmap_file{
  int mapid;
  struct file* file;
  struct list_elem elem;
  struct list vme_list;
};


void vm_destory(struct hash *vm);
void vm_init(struct hash* vm);
struct vm_entry* find_vme(void *addr);
bool insert_vme(struct hash* vm, struct vm_entry *vme);
bool delete_vme(struct hash* vm, struct vm_entry *vme);
struct vm_entry* vm_entry_alloc(void* addr);

static unsigned vm_hash_func(const struct hash_elem *e, void* aux);
static bool vm_less_func(const struct hash_elem *e1, const struct hash_elem *e2, void* aux);
static void vm_destory_func(struct hash_elem* e, void *aux);

#endif /* vm/vm.h */
