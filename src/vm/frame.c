#include "vm/frame.h"
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

struct list frame_list;
struct lock frame_list_lock;
struct list_elem* frame_clock;
int iter;

bool load_file(void *faddr, struct vm_entry* vme){
  off_t read_bytes = file_read_at (vme->file, faddr, vme->file_bytes, vme->file_offset);
  off_t zero_bytes = PGSIZE - read_bytes;
  memset(faddr + read_bytes, 0, zero_bytes);
  return read_bytes == (int)vme->file_bytes;
}

void frame_list_init (){
  list_init(&frame_list);
  lock_init(&frame_list_lock);
  iter = 0;
  frame_clock = NULL;
}

void add_frame_to_frame_list(struct frame* frame){
  lock_acquire(&frame_list_lock);
  list_push_back(&frame_list, &frame->elem);
  lock_release(&frame_list_lock);
}

void del_frame_from_frame_list(struct frame* frame){
  lock_acquire(&frame_list_lock);
  list_remove(&frame->elem);
  lock_release(&frame_list_lock);
}

struct frame* alloc_frame(enum palloc_flags flags){
  void* faddr = palloc_get_page(flags);
  //printf("alloc_frame start!\n");
  struct frame* p = malloc(sizeof *p);
  if(p == NULL){
    return NULL;
  }
  while(faddr == NULL){
    //pinned add later
    struct list_elem *e = get_next_frame_clock();
    frame_clock = list_next(e);
    struct frame* p_ = list_entry(e, struct frame, elem);
    //demand paging
    switch(p_->vme->type){
      case VM_BIN:
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          p_->vme->used_index = swap_out(p_->faddr);
          p_->vme->type = VM_ANON;
        }
        break;
      case VM_FILE:
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          lock_acquire(&frame_list_lock);
          file_write_at(p_->vme->file, p_->faddr, p_->vme->file_bytes, p_->vme->file_offset);
          lock_release(&frame_list_lock);
        }
        break;
      case VM_ANON:
        p_->vme->used_index = swap_out(p_->faddr);
        break;
    }
    p_->vme->is_loaded = false;
    free_frame(p_->faddr);
    faddr = palloc_get_page(flags);
  }
  p->faddr = faddr;
  p->thread = thread_current();

  add_frame_to_frame_list(p);
  //printf("alloc_frame end!\n");
  return p;
}


void free_frame(void *faddr){
  struct list_elem* e;
  for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame* p = list_entry(e, struct frame, elem);
    if(p->faddr == faddr){
      pagedir_clear_page(p->thread->pagedir, p->vme->vaddr);
      del_frame_from_frame_list(p);
      palloc_free_page(p->faddr);
      free(p);
      return;
    }
  }
}

static struct list_elem* get_next_frame_clock(){
  if(frame_clock == NULL){
    return list_begin(&frame_list);
  }
  if(frame_clock == list_end(&frame_list)){
    return list_begin(&frame_list);
  }
  return list_next(frame_clock);
}
