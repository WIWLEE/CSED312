#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/vm.h"
#include <list.h>
#include "threads/palloc.h"

struct frame{
  void *faddr;
  struct vm_entry *vme; // 
  struct thread *thread; // frame이 들어있는 thread를 가리킨다.
  struct list_elem elem; // frame이 frame list로 작동하게 한다.
};


bool load_file(void *kaddr, struct vm_entry* vme);
void frame_list_init ();
void add_frame_to_frame_list(struct frame* frame);
void del_frame_from_frame_list(struct frame* frame);
struct frame* alloc_frame(enum palloc_flags flags);
void free_frame(void *kaddr);
static struct list_elem* get_next_frame_clock();

#endif /* vm/frame.h */
