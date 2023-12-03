#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct bitmap *swap_bitmap;

struct lock swap_lock;

void swap_init(){
  lock_init(&swap_lock);
  swap_bitmap = bitmap_create(8 * 1024); // 8 * 1024 만큼의 bitmap을 만든다.
}


// page fault가 발생할 때, swap disk의 page를 새 frame에 할당한다.
// block.h의 block structure를 활용한다. 
// 
void swap_in(size_t used_index, void* kaddr){
  struct block *swap_device;
  lock_acquire(&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  block_sector_t s = used_index * SECTORS_PER_PAGE;
  int i;
  for(i = 0; i < SECTORS_PER_PAGE; i++){ // swap_device를 읽는다.
    block_read(swap_device, s + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }
  // bitmap을 setting한다.
  bitmap_set_multiple (swap_bitmap, used_index, 1, false);
  lock_release(&swap_lock);
}
 
// 선택한 evicting page를 swap disk로 복사하여 free frame을 얻는다.
size_t swap_out(void* kaddr){
  struct block *swap_device;
  lock_acquire(&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  size_t t = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

  if(t == BITMAP_ERROR){
    return BITMAP_ERROR;
  }
  int i;
  block_sector_t s = t * SECTORS_PER_PAGE;
  for(i = 0 ; i < SECTORS_PER_PAGE; i++){
    block_write(swap_device, s + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }

  lock_release(&swap_lock);
  return t;
}

void swap_set(size_t index){
  lock_acquire(&swap_lock);
  bitmap_set_multiple(swap_bitmap, index, 1, false); // index 비트를 false로 설정함
  lock_release(&swap_lock);
}
