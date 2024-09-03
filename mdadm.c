#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

void translate_address(uint32_t linear_addr, int *disk_num, int *block_num, int *offset){

	*disk_num = linear_addr / JBOD_DISK_SIZE;
	*block_num = (linear_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE ;
	*offset = (linear_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

uint32_t encode_operation(jbod_cmd_t cmd, int disk_num, int block_num){

	uint32_t op = cmd << 26 | disk_num << 22 | block_num;
	return op;
}

int mounted = 0;


int mdadm_mount(void) {
	int disk_num,block_num,offset;
	
	translate_address(JBOD_MOUNT,&disk_num,&block_num,&offset);
	uint32_t op = encode_operation(JBOD_MOUNT, disk_num, block_num);
	
	if (jbod_operation(op,NULL)==0){
		mounted = 1;
		return 1;
	}
	else if(jbod_operation(op,NULL)==-1){
  		return -1;
	}
	return 0;
}

int mdadm_unmount(void) {
	int disk_num,block_num,offset;

	translate_address(JBOD_UNMOUNT,&disk_num,&block_num,&offset);
	uint32_t op = encode_operation(JBOD_UNMOUNT, disk_num, block_num);

	if (jbod_operation(op,NULL)==0){
		mounted = 0;
		return 1;
	}
	else if(jbod_operation(op,NULL)==-1){
		return -1;
	}
	return 0;
}


int seek(int disk_num, int block_num){

	uint32_t op = encode_operation(JBOD_SEEK_TO_DISK, disk_num,0);
	jbod_operation(op,NULL);
	op = encode_operation(JBOD_SEEK_TO_BLOCK, 0, block_num);
	jbod_operation(op,NULL);
	return 0;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {

	int disk_num, block_num, offset;
	translate_address(addr,&disk_num,&block_num,&offset);


	//if the length is zero and buffer is NULL operation should succeed
	if (len == 0 && buf == NULL){

		return 0;

	}

	//if the length is not zero and buffer is NULL operation should fail
	if (len != 0 && buf == NULL){

		
		return -1;

	}

	//if the address is out-of-bounds the operation should fail
	if (addr + len >= JBOD_NUM_DISKS * JBOD_DISK_SIZE){

		
		return -1;
	}

	//if the length is greater than 1024 operation should fail
	if (len >1024){

		
		return -1;
	}

	//if the disk is unmounted operation should fail

	if (mounted == 0){
		
		return -1;
	}


	int curr_addr = addr - (addr % JBOD_BLOCK_SIZE);

	int initblock = block_num;

	int curr = 0;

	int disk_end = (addr + len) % JBOD_DISK_SIZE / JBOD_BLOCK_SIZE;
	


	while (curr_addr <= addr + len){

		//seek
		translate_address(curr_addr,&disk_num,&block_num,&offset);
		seek(disk_num,block_num);

		//read
		uint8_t tmp[JBOD_BLOCK_SIZE];
		jbod_operation(encode_operation(JBOD_READ_BLOCK,0,0),tmp);

 
		//process data
		
        int offset = 0;
        int size = JBOD_BLOCK_SIZE;

        if (block_num == initblock) {
          offset = addr % JBOD_BLOCK_SIZE;
          if (!offset) {
            if (len <= 256) {
              size = len;
            } else {
              size = 256;
            }
          } else {
            if (len <= 256 - offset) {
              size = len;
            } else {
              size = 256 - offset;
            }
          }
        } else if (block_num == disk_end) {
          offset = 0;
          size = (addr + len) % 256;
        }

        memcpy(buf + curr, tmp + offset, size);
        curr += size;
        curr_addr += 256;
      }



  return len;
}



int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {

	int disk_num, block_num, offset;
	translate_address(addr,&disk_num,&block_num,&offset);


	//if the length is zero and buffer is NULL operation should succeed
	if (len == 0 && buf == NULL){

		return 0;

	}

	//if the length is not zero and buffer is NULL operation should fail
	if (len != 0 && buf == NULL){

		
		return -1;

	}

	//if the address is out-of-bounds the operation should fail
	if (addr + len > JBOD_NUM_DISKS * JBOD_DISK_SIZE){

		
		return -1;
	}

	//if the length is greater than 1024 operation should fail
	if (len >1024){

		
		return -1;
	}

	//if the disk is unmounted operation should fail

	if (mounted == 0){
		
		return -1;
	}


	int curr_addr = addr - (addr % JBOD_BLOCK_SIZE);

	int initblock = block_num;

	int curr = 0;

	int disk_end = (addr + len) % JBOD_DISK_SIZE / JBOD_BLOCK_SIZE;
	

		while (curr_addr <= addr + len){

		//seek
		translate_address(curr_addr,&disk_num,&block_num,&offset);
		seek(disk_num,block_num);

		//read
		uint8_t tmp[JBOD_BLOCK_SIZE];
		jbod_operation(encode_operation(JBOD_READ_BLOCK,0,0),tmp);
 
		//process data
		
        int offset = 0;
        int size = JBOD_BLOCK_SIZE;

        if (block_num == initblock) {
          offset = addr % JBOD_BLOCK_SIZE;
          if (!offset) {
            if (len <= 256) {
              size = len;
            } else {
              size = 256;
            }
          } else {
            if (len <= 256 - offset) {
              size = len;
            } else {
              size = 256 - offset;
            }
          }
        } else if (block_num == disk_end) {
          offset = 0;
          size = (addr + len) % 256;
        }

        memcpy(tmp + offset,buf + curr, size);
		translate_address(curr_addr,&disk_num,&block_num,&offset);
		seek(disk_num,block_num);
		jbod_operation(encode_operation(JBOD_WRITE_BLOCK,0,0), tmp);
		        
		curr += size;
        curr_addr += 256;
		} 


  return len;
}
