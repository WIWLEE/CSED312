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

//vme->file(파일)에서 faddr(버퍼)로 파일을 읽어온다.
//read_bytes와 zero_bytes의 합이 PGSIZE가 되게끔 조정해 준다.
bool load_file(void *faddr, struct vm_entry* vme){
  off_t read_bytes = file_read_at (vme->file, faddr, vme->file_bytes, vme->file_offset); 
  off_t zero_bytes = PGSIZE - read_bytes;
  memset(faddr + read_bytes, 0, zero_bytes);
  return read_bytes == (int)vme->file_bytes;
}

//frame table을 initialization한다.
void frame_list_init (){
  list_init(&frame_list);
  lock_init(&frame_list_lock);
  frame_clock = NULL;
}

//frame을 frame table에 추가한다.
void add_frame_to_frame_list(struct frame* frame){
  lock_acquire(&frame_list_lock);
  list_push_back(&frame_list, &frame->elem);
  lock_release(&frame_list_lock);
}

//frame을 frame table에서 지운다.
void del_frame_from_frame_list(struct frame* frame){
  lock_acquire(&frame_list_lock);
  list_remove(&frame->elem);
  lock_release(&frame_list_lock);
}

//frame을 alloc한다. flags는 PAL_ZERO, PAL_USER 등이다.
struct frame* alloc_frame(enum palloc_flags flags){
  void* faddr = palloc_get_page(flags);
  struct frame* p = malloc(sizeof *p); //frame의 크기만큼 할당한다.
  if(p == NULL){
    return NULL;
  }

  //free physical memory가 없으면 eviction 및 재할당
  while(faddr == NULL){
    
    struct list_elem *e = get_next_frame_clock();
    frame_clock = list_next(e);
    struct frame* p_ = list_entry(e, struct frame, elem); // frame list에서 e 위치에 있는 frame을 가져온다

    //frame allocation 시 evict 되는 부분
    //demand paging을 시행한다.
    switch(p_->vme->type){
      case VM_BIN: // dirty bit가 1이면 스왑 파티션에 기록한 후 페이지를 해제
        // 페이지를 해제한 후 요구 페이징을 위해 타입을 VM_ANON으로 변경
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          p_->vme->used_index = swap_out(p_->faddr);
          p_->vme->type = VM_ANON;
        }
        break; // dirty bit가 1이면 파일에 변경 내용을 저장한 후 페이지를 해제, dirty bit가 0이면 바로 페이지를 해제
      case VM_FILE:
        if(pagedir_is_dirty(thread_current()->pagedir, p_->vme->vaddr)){
          lock_acquire(&frame_list_lock);
          file_write_at(p_->vme->file, p_->faddr, p_->vme->file_bytes, p_->vme->file_offset);
          lock_release(&frame_list_lock);
        }
        break;
      case VM_ANON: // 항상 스왑 파티션에 기록
        p_->vme->used_index = swap_out(p_->faddr);
        break;
    }
    //eviction되었으니 loaded false만들고 frame도 free시킨다. 
    p_->vme->is_loaded = false;
    free_frame(p_->faddr);
    faddr = palloc_get_page(flags);
  }
  p->faddr = faddr;
  p->thread = thread_current();

  //frame table에 할당한 Frame 추가하기
  add_frame_to_frame_list(p);
  return p;
}


void free_frame(void *faddr){
  struct list_elem* e;
  for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
    struct frame* p = list_entry(e, struct frame, elem);
    if(p->faddr == faddr){
      pagedir_clear_page(p->thread->pagedir, p->vme->vaddr); // page directory에서도 지운다. (해당 thread의 pagedir에서)
      del_frame_from_frame_list(p); // 프레임 리스트에서도 지운다
      palloc_free_page(p->faddr);
      free(p);
      return;
    }
  }
}

// frame들의 list를 타고 쭉 진행되는 함수이다. 
static struct list_elem* get_next_frame_clock(){
  if(frame_clock == NULL){
    return list_begin(&frame_list);
  }
  if(frame_clock == list_end(&frame_list)){
    return list_begin(&frame_list);
  }
  return list_next(frame_clock);
}
