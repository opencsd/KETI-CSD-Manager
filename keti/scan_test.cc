#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
using namespace std;
int main(){
    for(int i=0; i<1050; i++){


  char block_buf[40960];  

  int dev_fd = open("/dev/ngd-blk", O_RDONLY);
  uint64_t block_offset = 143845297720;
  int block_size = 4023;
  
  lseek(dev_fd,block_offset,SEEK_SET);
  int read_size = read(dev_fd, block_buf, block_size);
  char* iter = block_buf;
  string asd = "asdasdasdasdasdasdasdasdasdasdasdasdasdasd";
  memcpy(iter + 4023,asd.c_str(),43);

  std::cout << "#buffer_read_size : ["<< i <<"] " << read_size << std::endl;
//   for(int i=0; i<block_size; i++){
//     printf("%02X",(u_char)block_buf[i]);
//   }
  std::cout << std::endl;
  close(dev_fd);
    }
}
