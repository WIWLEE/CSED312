#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
bool is_validate_addr(void *addr);
bool is_validate_addr_all(void *addr, int size);
struct lock file_lock;

void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool is_validate_addr(void *addr){
  if (addr == NULL || addr >= PHYS_BASE){
    return false;
  }
  else return true;
}

bool is_validate_addr_all(void *addr, int size)
{
  for(int i=0;i<size;i++)
    if(is_validate_addr((void *)addr+i) == false){
      return false;
    }
  return true;
}

static void
syscall_handler (struct intr_frame *f)
{
  void *fesp = f->esp;
  if(is_validate_addr(fesp) == false){
    exit(-1);
  }

  switch(*(uint32_t *)fesp){
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      if(is_validate_addr_all((fesp + 4), sizeof(uint32_t)) == false){
        exit(-1);
      };
      exit(*(uint32_t *)(fesp + 4));
      break;
    case SYS_EXEC:
      f->eax = exec((const char *)*(uint32_t *)(fesp+4));
      break;
    case SYS_WAIT:
      if(is_validate_addr_all((fesp + 4), sizeof(pid_t)) == false){
        exit(-1);
      };
      f->eax = wait(*(pid_t*)(fesp+4));
      break;
    case SYS_CREATE:
      f->eax = create((const char *)*(uint32_t *)(fesp + 4), (const char *)*(uint32_t *)(fesp + 8));
      break;
    case SYS_REMOVE:
      f->eax = remove((const char *)*(uint32_t *)(fesp + 4));
      break;
    case SYS_OPEN:
      f->eax = open((const char *)*(uint32_t *)(fesp + 4));
      break;
    case SYS_FILESIZE:
      if(is_validate_addr_all((fesp + 4), sizeof(uint32_t)) == false){
        exit(-1);
      };
      f->eax = filesize((int)*(uint32_t *)(fesp + 4));
      break;
    case SYS_READ:
      f->eax = read((int)*(uint32_t *)(fesp + 4), (void *)*(uint32_t *)(fesp + 8), (unsigned)*((uint32_t *)(fesp + 12)));
      break;
    case SYS_WRITE:
      f->eax = write((int)*(uint32_t *)(fesp + 4), (void *)*(uint32_t *)(fesp + 8), (unsigned)*((uint32_t *)(fesp + 12)));
      break;
    case SYS_SEEK:
      if(is_validate_addr_all((fesp + 4), sizeof(uint32_t)) == false || is_validate_addr_all((fesp + 8), sizeof(uint32_t))==false){
        exit(-1);
      };
      seek((int)*(uint32_t *)(fesp + 4), (unsigned)*((uint32_t *)(fesp + 8)));
      break;
    case SYS_TELL:
     if(is_validate_addr_all((fesp + 4), sizeof(uint32_t)) == false || is_validate_addr_all((fesp + 8), sizeof(uint32_t))==false){
        exit(-1);
      };
      f->eax = tell((int)*(uint32_t *)(fesp + 4));
      break;
    case SYS_CLOSE:
      if(is_validate_addr_all((fesp + 4), sizeof(uint32_t)) == false || is_validate_addr_all((fesp + 8), sizeof(uint32_t))==false){
        exit(-1);
      };
      close((int)*(uint32_t *)(fesp + 4));
      break;
    default:
      exit(-1);
  }
}

void halt() {
  shutdown_power_off();
}

void exit(int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
}


pid_t exec (const char *file) {
  if (file == NULL || is_validate_addr_all(file, sizeof(file)) == false) {
    exit (-1);
  }
  else return process_execute(file);
}

int wait (pid_t pid) {
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
  
 if (file == NULL || is_validate_addr_all(file,sizeof(file)) == false) {
    exit (-1);
  }
  else return filesys_create (file, initial_size);
}

bool remove (const char *file){
 if (file == NULL || is_validate_addr_all(file, sizeof(file)) == false) {
    exit (-1);
  }
  else return filesys_remove (file);
}

int open (const char *file){
  if (file == NULL || is_validate_addr_all(file, sizeof(file)) == false) {
    exit (-1);
  }
  
  lock_acquire(&file_lock);
  struct file* open_file = filesys_open(file);

  if(open_file == NULL){
    lock_release(&file_lock);
    return -1;
  }
  else{
    int cur_fd = 3; //0,1,2 -> stdin,out,err
    int open_fd; //열린 fd 값

    while(cur_fd<128){
      //비어있는 file descriptor 위치 찾기
      if(thread_current()->file_descriptor[cur_fd] == NULL){
        
        //deny write when execution file
        if(strcmp(thread_current()->name, file) == 0){
          file_deny_write(open_file);
        }

        //fd에 쓰기
        thread_current()->file_descriptor[cur_fd] = open_file;
        open_fd = cur_fd;
        break;
      }
      cur_fd++;
    }

    lock_release(&file_lock);
    //열린 fd 반환
    return open_fd;
  }
}

int filesize (int fd){
  if(thread_current()->file_descriptor[fd] == NULL){
    return -1;
  }
  else{
    return file_length(thread_current()->file_descriptor[fd]);
  }
}

int read (int fd, void *buffer, unsigned size){

  if (is_validate_addr_all(buffer, sizeof(buffer)) == false) {
    exit (-1);
  }

  //read from stdin or error(stdout) or file?
  if(fd == 0){
    lock_acquire(&file_lock);
    int byte = input_getc();
    lock_release(&file_lock);
    return byte;
  }
  else if(fd == 1){
    return -1;
  }
  else if(fd > 2 && fd<128){
    lock_acquire(&file_lock);
    struct file* file = thread_current()->file_descriptor[fd];
    int byte = 0;
    if(file != NULL){
      byte = file_read(file, buffer, size); 
    }
    lock_release(&file_lock);
    return byte;
  }
}

int write (int fd, const void *buffer, unsigned size){
 if (is_validate_addr_all(buffer,sizeof(buffer)) == false) {
    exit (-1);
  }

  //error(stdin) or stdout or file?
  if (fd == 0){
    return -1;
  }
	else if (fd == 1) {
		lock_acquire(&file_lock);
		putbuf(buffer, size);
		lock_release(&file_lock);
		return size;
	}
  else if(fd > 2 && fd<128){
		lock_acquire(&file_lock);
    struct file* file = thread_current()->file_descriptor[fd];
    int byte = 0;
    if(file != NULL){
      byte = file_write(file, buffer, size);
    }
		lock_release(&file_lock);
		return byte;
	}
}

void seek (int fd, unsigned position){
  if(thread_current()->file_descriptor[fd] != NULL){
    file_seek(thread_current()->file_descriptor[fd], position);
  }
}

unsigned tell (int fd){
  if(thread_current()->file_descriptor[fd] != NULL){
    return file_tell(thread_current()->file_descriptor[fd]);
  }
  else{
    exit(-1);
  }
}

void close (int fd){
  if(thread_current()->file_descriptor[fd] != NULL){
    lock_acquire(&file_lock);
    if(thread_current()->file_descriptor[fd] != NULL){
      file_close(thread_current()->file_descriptor[fd]);
    }
    thread_current()->file_descriptor[fd] = NULL;
    lock_release(&file_lock);
  }
}

