#include "vm/vm.h"
#include "vm/swap.h"
#include <bitmap.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/block.h"

static unsigned vm_hash_func(const struct hash_elem *e, void* aux);
static bool vm_less_func(const struct hash_elem *e1, const struct hash_elem *e2, void* aux);
static void vm_destory_fun(struct hash_elem* e, void *aux);

void vm_destory(struct hash *vm){
  hash_destroy(vm, vm_destory_fun);
  free(vm);
}

void vm_init(struct hash* vm){
  hash_init(vm, vm_hash_func, vm_less_func, NULL);
}


struct vm_entry* find_vme(void *addr){
  struct vm_entry v;
  struct hash_elem *e;
  if(thread_current()->vm == NULL)
    return NULL;
  v.vaddr = (void *)pg_round_down (addr);

  e = hash_find(thread_current()->vm, &v.elem);
  if(e == NULL)
    return NULL;
  return hash_entry(e, struct vm_entry, elem);
}

bool insert_vme(struct hash* vm, struct vm_entry *vme){
  struct hash_elem *e = hash_insert(vm, &vme->elem);
  if (e == NULL)
    return true;
  return false;
}

bool delete_vme(struct hash* vm, struct vm_entry *vme){
  struct hash_elem *e = hash_delete(vm, &vme->elem);
  if (e==NULL)
    return false;
  free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
  if(vme->used_index != -1 || vme->used_index != BITMAP_ERROR){
    swap_set(vme->used_index);
  }
  free(vme);
  return true;
}

struct vm_entry* vm_entry_alloc(void* addr){
  struct vm_entry* v = malloc(sizeof *v);
  if(v!= NULL){
    v->vaddr = pg_round_down(addr);
    v->thread = thread_current();

    v->file = NULL;
    v->file_offset = 0;
    v->file_bytes = 0;
    v->used_index = -1;

  }
  return v;
}

static unsigned vm_hash_func(const struct hash_elem *e, void* aux UNUSED){
  const struct vm_entry* v = hash_entry(e, struct vm_entry, elem);
  return ((uintptr_t) v->vaddr) >> PGBITS;
}

static bool vm_less_func(const struct hash_elem *e1, const struct hash_elem *e2, void* aux UNUSED){
  const struct vm_entry *e1_ = hash_entry (e1, struct vm_entry, elem);
  const struct vm_entry *e2_ = hash_entry (e2, struct vm_entry, elem);
  return e1_->vaddr < e2_->vaddr;
}

static void vm_destory_fun(struct hash_elem* e, void *aux UNUSED){
  struct vm_entry *v = hash_entry(e, struct vm_entry, elem);
  free_page(pagedir_get_page(thread_current()->pagedir, v->vaddr));
  if(v->used_index != -1){
    swap_set(v->used_index);
  }
  free(v);
}
