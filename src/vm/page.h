#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/vm.h"
#include <list.h>
#include "threads/palloc.h"

struct page{
  void *kaddr;
  struct vm_entry *vme;
  struct thread *thread;
  struct list_elem lru;
};

struct mmap_file{
  int mapid;

  struct file* file;
  struct list_elem elem;
  struct list vme_list;
};

bool load_file(void *kaddr, struct vm_entry* vme);
void lru_list_init ();
void add_page_to_lru_list(struct page* page);
void del_page_from_lru_list(struct page* page);
struct page* alloc_page(enum palloc_flags flags);
void free_page(void *kaddr);
static struct list_elem* get_next_lru_clock();

#endif /* vm/page.h */
