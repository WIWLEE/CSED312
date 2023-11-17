#include "vm/page.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct list lru_list;
struct lock lru_list_lock;
struct list_elem* lru_clock;
int iter;

bool load_file(void *kaddr, struct vm_entry* vme){
  off_t read_bytes = file_read_at (vme->file, kaddr, vme->file_bytes, vme->file_offset);
  off_t zero_bytes = PGSIZE - read_bytes;
  memset(kaddr + read_bytes, 0, zero_bytes);
  return read_bytes == (int)vme->file_bytes;
}

void lru_list_init (){
  list_init(&lru_list);
  lock_init(&lru_list_lock);
  iter = 0;
  lru_clock = NULL;
}

void add_page_to_lru_list(struct page* page){
  lock_acquire(&lru_list_lock);
  list_push_back(&lru_list, &page->lru);
  lock_release(&lru_list_lock);
}

void del_page_from_lru_list(struct page* page){
  lock_acquire(&lru_list_lock);
  list_remove(&page->lru);
  lock_release(&lru_list_lock);
}

struct page* alloc_page(enum palloc_flags flags){
  void* kaddr = palloc_get_page(flags);
  //printf("alloc_page start!\n");
  struct page* p = malloc(sizeof *p);
  if(p == NULL){
    return NULL;
  }
  while(kaddr == NULL){
    //pinned add later
    struct list_elem *e = get_next_lru_clock();
    lru_clock = list_next(e);
    struct page* p_ = list_entry(e, struct page, lru);
    //demand paging
    switch(p_->vme->type){
      case VM_BIN:
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          p_->vme->used_index = swap_out(p_->kaddr);
          p_->vme->type = VM_ANON;
        }
        break;
      case VM_FILE:
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          lock_acquire(&lru_list_lock);
          file_write_at(p_->vme->file, p_->kaddr, p_->vme->file_bytes, p_->vme->file_offset);
          lock_release(&lru_list_lock);
        }
        break;
      case VM_ANON:
        p_->vme->used_index = swap_out(p_->kaddr);
        break;
    }
    p_->vme->is_loaded = false;
    free_page(p_->kaddr);
    kaddr = palloc_get_page(flags);
  }
  p->kaddr = kaddr;
  p->thread = thread_current();

  add_page_to_lru_list(p);
  //printf("alloc_page end!\n");
  return p;
}


void free_page(void *kaddr){
  struct list_elem* e;
  for (e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e)){
    struct page* p = list_entry(e, struct page, lru);
    if(p->kaddr == kaddr){
      pagedir_clear_page(p->thread->pagedir, p->vme->vaddr);
      del_page_from_lru_list(p);
      palloc_free_page(p->kaddr);
      free(p);
      return;
    }
  }
}

static struct list_elem* get_next_lru_clock(){
  if(lru_clock == NULL){
    return list_begin(&lru_list);
  }
  if(lru_clock == list_end(&lru_list)){
    return list_begin(&lru_list);
  }
  return list_next(lru_clock);
}
