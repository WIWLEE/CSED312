#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/vaddr.h"
#include <stdio.h>

void swap_init(void); // swap 영역을 초기화
void swap_in(size_t used_index, void* kaddr); // 특정 index의 swap slot 데이터를 address로 복사
size_t swap_out(void* kaddr); // addr가 가리키는 page를 swap partition에 저장, 저장된 swap slot의 index를 return

#endif /* vm/swap.h */
