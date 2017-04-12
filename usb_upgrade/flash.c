/*
 *Filename generate_upgrade_file.c
 *
 *Description:generate a format file
 *
 *Author:rutk1t0r
 *
 *Date:2017.4.12
 *
 *GPL
 *
 *region and non_region erase algorithm referrence :http://projects.qi-hardware.com/svn/ingenic-linux-02os-linux-2-6-24-3/trunk/drivers/mtd/mtd-utils/flash_erase.c
 */

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>



#define HASH_SCALE 7

#define BLOCK_BUFFER_SIZE (128*1024)
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

int check_sum_partition(char *fname, char *partition_name, unsigned int *partition_offset, unsigned int *partition_size)
{
	
	int readlen = BLOCK_BUFFER_SIZE;
	upgrade_file_header *ufh = NULL;
	partition_section_info_t *section_header = NULL;
	int i = 0, j = 0;
	unsigned int save_check_sum = 0, calc_check_sum = 0;
	int ifd = -1;
	char *buffer = NULL;
	unsigned int offset = 0, size = 0,total_len = 0;
	int calc_size = 0;
	int section_num = 0;
	int current = 0, read_cnt = 0;
	if(fname == NULL || partition_name == NULL || partition_offset == NULL || partition_size == NULL)
	{
		return -1;
	}
	if((ifd = open(fname, O_RDONLY)) == -1)
	{
		printf("open input file error!\n");
		return -1;
	}
	
	buffer = (char *)malloc(readlen);
	if(buffer == NULL)
	{
		perror("malloc error");
		close(ifd);
		return -1;
	}
	//first check the img,we use the buffer to calc check_sum
	//we must ensure the generate_upgrade_file and this file Big/Little Endian are Consistent 
	if(read(ifd, buffer,sizeof(*ufh)) != sizeof(*ufh))
	{
		perror("read img file error");
		goto error;
	}
	ufh = (upgrade_file_header*)buffer;
	
	if(sizeof(partition_section_info_t) != ufh->section_size)
	{
		printf("partition size error\n");
		goto error;
	}
	section_num = ufh->section_num;

	memset(buffer, 0x00, readlen);
	if(read(ifd, buffer, sizeof(partition_section_info_t)*section_num) != sizeof(partition_section_info_t)*section_num)
	{
		perror("read partition error");
		goto error;
	}
	section_header = (partition_section_info_t*)buffer;
	for(i = 0; i<section_num; ++i)
	{
		if(strncmp(partition_name, section_header[i].name, 8) == 0)
		{
			break;
		}
	}
	
	if(i == section_num)
	{
		printf("partition_name error\n");
		goto error;
	}
	save_check_sum = section_header[i].check_sum;
	for(j = 0; j<i; ++j)
	{
		offset += section_header[j].size;
	}

	size = section_header[i].size;
	*partition_size = size;
	*partition_offset =  sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*section_num+offset ;
	if(lseek(ifd, sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*section_num+offset, SEEK_SET) == -1)
	{
		perror("lseek error");
		goto error;
	}
	memset(buffer, 0x00, readlen);
	total_len = size;
	//printf("offset %d  size %d\n", offset, size);
#if 1
	
	while(size != 0)
	{
		if(size < readlen)
		{
			readlen = size;
		}
		if((read_cnt = read(ifd, buffer, readlen)) != -1)
		{
			calc_check_sum += simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE);
			current += read_cnt;
			size -= read_cnt;
			memset(buffer, 0x00, readlen);
			printf("check complete:%f%%\n ", 100*(double)current/(double)total_len);
		}
		else
		{
			perror("read error");
			goto error;
		}

		
	}

	if(save_check_sum == calc_check_sum)
	{
		printf("check sum is OK\n");
		close(ifd);
		free(buffer);
		return 0;
	}
#endif
error:
	close(ifd);
	free(buffer);
	return -1;

}



int region_erase(int fd, int start, int count, int unlock, int regcount)
{
	int i, j;
	region_info_t * reginfo = NULL;

	reginfo = calloc(regcount, sizeof(region_info_t));
	if(reginfo == NULL)
	{
		perror("calloc error");
		return -1;
	}
	for(i = 0; i < regcount; i++)
	{
		reginfo[i].regionindex = i;
		if(ioctl(fd,MEMGETREGIONINFO,&(reginfo[i])) != 0)
			goto error;
		else
			printf("Region %d is at %d of %d sector and with sector "
					"size %x\n", i, reginfo[i].offset, reginfo[i].numblocks,
					reginfo[i].erasesize);
	}

	// We have all the information about the chip we need.

	for(i = 0; i < regcount; i++)
	{ //Loop through the regions
		region_info_t * r = &(reginfo[i]);

		if((start >= reginfo[i].offset) &&
				(start < (r->offset + r->numblocks*r->erasesize)))
			break;
	}

	if(i >= regcount)
	{
		printf("Starting offset %x not within chip.\n", start);
		goto error;
	}

	//We are now positioned within region i of the chip, so start erasing
	//count sectors from there.

	for(j = 0; (j < count)&&(i < regcount); j++)
	{
		erase_info_t erase;
		region_info_t * r = &(reginfo[i]);

		erase.start = start;
		erase.length = r->erasesize;

		if(unlock != 0)
		{ //Unlock the sector first.
			if(ioctl(fd, MEMUNLOCK, &erase) != 0)
			{
				perror("\nMTD Unlock failure");
				
				goto error;
			}
		}
		printf("\rPerforming Flash Erase of length 0x%llx at offset 0x%llx",
				erase.length, erase.start);
		fflush(stdout);
		if(ioctl(fd, MEMERASE, &erase) != 0)
		{
			perror("\nMTD Erase failure");
			
			goto error;
		}


		start += erase.length;
		if(start >= (r->offset + r->numblocks*r->erasesize))
		{ //We finished region i so move to region i+1
			printf("\nMoving to region %d\n", i+1);
			i++;
		}
	}

	printf(" done\n");
	return 0;
error:
	free(reginfo);
	return -1;
}

int non_region_erase(int fd, int start, int count, int unlock)
{
	mtd_info_t meminfo;
	
	if(fd < 0 || start < 0 || count <= 0)
	{
		return -1;
	}
	memset((char*)&meminfo, 0x00, sizeof(meminfo));
	if(ioctl(fd, MEMGETINFO, &meminfo) == 0)
	{
		int total_len = meminfo.size;
		int current = 0;
		erase_info_t erase;
		erase.start = start;
		erase.length = meminfo.erasesize;
		for(; count>0; count--)
		{
			
			
			if(unlock != 0)
			{
				//Unlock the sector first
				printf("Performing Flash unlock at offset 0x%x\n", erase.start);
				if(ioctl(fd, MEMUNLOCK, &erase) != 0)
				{
					perror("MTD Unlock failure\n");
					return -2;
				}
			}
			if(ioctl(fd, MEMERASE, &erase) != 0)
			{
				perror("MTD Erase failure\n");
				return -3;
			}
			erase.start += meminfo.erasesize;
			current += erase.length;
			printf("Performed Flash Erase of length %u at offset 0x%x,process:%4f%%\n",
			erase.length, erase.start,100*(double)current/(double)total_len);
			fflush(stdout);
		}
		printf("done\n");
		
	}
	return 0;
}


int main(int argc, char *argv[])
{
	int fd = -1, ifd = -1;
	char *cmd = NULL;
	char *upgrade_img_name = NULL;
	char *partition_name = NULL;
	struct mtd_info_user info;
	int regcount = 0;
	int res = 0, cnt = 0, write_cnt = 0;
	int imglen = 0, total_len = 0;
	int readlen = 0;
	char *write_buf = NULL;
	int percent = 0;
	if(3 > argc)
	{
		printf("You must specify a device!\n");
		return -1;
	}
	memset(&info, 0x00, sizeof(info));
	cmd = argv[1];
	if(argc > 4)
	{
		upgrade_img_name = argv[3];
		partition_name = argv[4];
	}
	//Open and size the device,argv[2] is like mtd0,1,2...
	if((fd = open(argv[2], O_RDWR))<0)
	{
		fprintf(stderr, "file open error\n");
		goto error;
	}
	else
	{
		if(ioctl(fd, MEMGETINFO, &info)!=0)
		{
			perror("get info error");
			goto error;
		}
		printf("info.size=0x%x\ninfo.erasesize=0x%x\ninfo.writesize=0x%x\ninfo.oobsize=0x%x\n",
		info.size,info.erasesize,info.writesize,info.oobsize);
	}
	if(ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0)
	{
		printf("regcount =%d\n", regcount);
	}
	else
	{
		perror("get regcount error");
		goto error;
	}
	/*erase the device*/
	if(strcmp(cmd, "erase") == 0)
	{
		if(regcount == 0)
		{
			if((res = non_region_erase(fd, 0, (info.size/info.erasesize), 0)) != 0)
			{
				perror("erase error\n");
			}
		}
		printf("erase\n");
	}
	if(strcmp(cmd, "write") == 0)
	{
		int partition_offset = 0, partition_size = 0;
		int read_size = BLOCK_BUFFER_SIZE;
		printf("Preparing write,checking...\n");
		
		if(check_sum_partition(upgrade_img_name, partition_name, &partition_offset, &partition_size)  != 0)
		{
			printf("check_sum calc error\n");
			goto error;
		}

		ifd = open(upgrade_img_name, O_RDONLY);
		write_buf = (char*)malloc(BLOCK_BUFFER_SIZE);
		if(write_buf == NULL)
		{
			perror("malloc error");
			goto error;
		}

		printf("Starting write:\n");
		total_len = partition_size;
		lseek(ifd, partition_offset, SEEK_SET);
		while(total_len != 0)
		{
			if(total_len < BLOCK_BUFFER_SIZE)
			{
				read_size = total_len;
			}
			memset(write_buf, 0xff, BLOCK_BUFFER_SIZE);
			if((cnt = read(ifd, write_buf, read_size)) != read_size)
			{
				if(cnt == -1)
				{
					perror("read error");
					goto error;
				}
				else
				{
					printf("length error\n");
					goto error;
				}
			}
			
			if((write_cnt = write(fd, write_buf, cnt)) != cnt)
			{
				printf("write mtd devices error,startaddr = 0x%x\n", ((partition_size)-total_len));
				goto error;
			}
			else
			{
				percent += write_cnt;
				printf("write mtd device Ok,startaddr = 0x%x complete:%4f%%\n ",((partition_size)-total_len), 100*(double)percent/(double)(partition_size));
				
			}
			
			total_len -= cnt;
			
		}
		printf("\n");
		printf("Write done\n");
		
		
	}
	
	if(strcmp(cmd, "read")==0)
	{
		printf("read\n");
		
	}

error:
	if(fd != -1)
	{
		close(fd);
	}
	if(ifd != -1)
	{
		close(ifd);
	}
	if(write_buf != NULL)
	{
		free(write_buf);
	}
	return -1;
	
}

