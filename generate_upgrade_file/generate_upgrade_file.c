#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define HASH_SCALE 7
typedef struct partition
{
	char name[8];        //partition name,ie:cfe,loader,kernel,rootfs
	char dev_name[8];    //mtd name,ie:mtd0,mtd1...
	char img_name[16];   //img file name,ie:cfe.bin...
	unsigned int version;
	unsigned int size;   //img file size
	unsigned int check_sum; //check_sum
}partition_section_info_t;


typedef struct upgrade_file_header
{
	unsigned int header_size;
	unsigned int section_num;
	unsigned int section_size;
	
}upgrade_file_header;
// 32bit arch
unsigned int simple_hash(char *buffer, unsigned int size, unsigned char scale)
{
	unsigned int *p = (unsigned int*)buffer;
	unsigned int sum = 0;
	
	if(buffer == NULL || size < sizeof(unsigned int) || scale == 0 || scale >= sizeof(unsigned int)*8)
	{
		return -1;
	}
	//per 4 bytes 
	while(size > 0)
	{
		sum += (*p >> scale) || (*p << (sizeof(unsigned int)*8-scale));
		size -= sizeof(unsigned int);
		p ++;
	}
	return sum;
}

int main(int argc, char **argv)
{
	int section_num = 0;
	int i = 0;
	int target_fd = -1;
	int input_fd = -1;
	partition_section_info_t *section_header = NULL;
	char *buffer = NULL;
	unsigned int block_size = 128*1024;
	upgrade_file_header ufh;
	memset(&ufh, 0x00, sizeof(ufh));
	printf("Please input the number of the section:");
	scanf("%d", &section_num); //suppose success
	section_header = (partition_section_info_t*)calloc(section_num, sizeof(partition_section_info_t));
	if(section_header == NULL)
	{
		perror("calloc error");
		return -1;
	}
	printf("Please input the section_info,name,dev_name,img_file_name,version for example: cfe mtd0 cfe.bin 1\n");
	for(; i<section_num; ++i)
	{
		printf("the %d info:",i);//don't let stackoverflow,so let the name shortly
		scanf("%s %s %s %u", section_header[i].name, section_header[i].dev_name,\
							 section_header[i].img_name, &section_header[i].version);
		
	}
	
	if((target_fd = open("upgrade_file", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
	{
		perror("target file open error");
		return -2;
	}
	
	//header
	ufh.header_size = sizeof(ufh);
	ufh.section_num = section_num;
	ufh.section_size = sizeof(partition_section_info_t);
	if(write(target_fd, &ufh, sizeof(ufh))!=sizeof(ufh))
	{
		perror("write file header error");
		return -4;
	}
	
	lseek(target_fd, sizeof(partition_section_info_t)*section_num+sizeof(ufh), SEEK_SET); //jmp the section region,pointer payload
																		 //because we need calc the check_sum and write the payload at the same time
	
	buffer = (char*)malloc(block_size);
	if(buffer == NULL)
	{
		perror("malloc memory error");
		return -5;
	}
	memset(buffer, 0x00, block_size);
	//full sections and write payloads
	for(i = 0; i<section_num; ++i)
	{
		int readlen = 0;
		int imglen = 0,total = 0,current = 0;
		int write_cnt = 0;
		unsigned int sum = 0;
		if((input_fd = open(section_header[i].img_name, O_RDONLY)) == -1)
		{
			perror("section file open error");
			return -3;
		}
		section_header[i].size = total = imglen = lseek(input_fd, 0, SEEK_END); //or use other syscall to get attribution
		lseek(input_fd, 0, SEEK_SET);
		
		while(imglen)
		{
			if((readlen = read(input_fd, buffer, block_size)) != block_size)
			{
				if(readlen < block_size)
				{
					printf("file end\n");
				}
				else
				{
					printf("file IO error");
					return -9;
				}
			}
			sum += simple_hash(buffer, block_size, HASH_SCALE); //calc check_sum
			if((write_cnt = write(target_fd, buffer, readlen)) != readlen)
			{
				perror("write target file error");
				return -10;
			}
			else
			{
				current += write_cnt;
				printf("the %s partition write complete %f%%\n", section_header[i].name, 100*(double)current/(double)total);
				
			}
			imglen -= readlen;
			memset(buffer, 0x00, block_size); //must be 
		}
		section_header[i].check_sum = sum;
		close(input_fd);
		input_fd = -1;
	}
	
	//now write the sections
	lseek(target_fd, sizeof(ufh), SEEK_SET);
	write(target_fd, section_header, sizeof(partition_section_info_t)*section_num);
	close(target_fd);
	free(section_header);
	return 0;
}