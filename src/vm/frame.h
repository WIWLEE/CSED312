#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/vm.h"
#include <list.h>
#include "threads/palloc.h"

struct frame{
  void *faddr; // physical addr = kernel addr
  struct vm_entry *vme; // 매칭된 virtual address의 vm_entry
  struct thread *thread; // frame이 들어있는 thread를 가리킨다.
  struct list_elem elem; // frame이 frame list로 작동하게 한다.
};


bool load_file(void *kaddr, struct vm_entry* vme); // 해당 파일 kaddr 로드
void frame_list_init (); // frame list 초기화
void add_frame_to_frame_list(struct frame* frame); // frame 추가
void del_frame_from_frame_list(struct frame* frame); // frame 삭제
struct frame* alloc_frame(enum palloc_flags flags); // frame 할당
void free_frame(void *kaddr); // frame 해제
static struct list_elem* get_next_frame_clock(); // 다음 LRU list_elem frame 찾기

#endif /* vm/frame.h */
