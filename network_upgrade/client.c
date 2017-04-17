/*
 *Filename: flash.c
 *
 *Description:erase mtd flash and write data via ioctl 
 *
 *Author:rutk1t0r
 *
 *Date:2017.4.12
 *
 *GPL
 *
 *region and non_region erase algorithm reference :http://projects.qi-hardware.com/svn/ingenic-linux-02os-linux-2-6-24-3/trunk/drivers/mtd/mtd-utils/flash_erase.c
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
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>


#define HASH_SCALE 7
#define UPGRADE_STR "upgrade"
#define BLOCK_BUFFER_SIZE (128*1024)

#define DEBUG  
#ifdef DEBUG  
#define debug(x) printf(x)  
#else  
#define debug(x)  
#endif  
#define SERVER_PORT 8888  
#define SERVER_IP "192.168.10.82"  
#define LISTEN_NUM 10  
typedef void (*sighandler_t)(int);  
  
void sigpipe_handle(int arg); //sigpipe信号处理函数，当send或是recv在等待发送或是接收数据时发现连接断开，系统会发出该信号 
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
		sum += (((*p) >> scale) | ((*p) << (sizeof(unsigned int)*8-scale)));
		size -= sizeof(unsigned int*);
		
	}
	return sum;
}

int check_sum_partition(char *fname)
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
	if(fname == NULL)
	{
		return -1;
	}
	if((ifd = open(fname, O_RDONLY)) == -1)
	{
		printf("open input file error!\n");
		return -1;
	}
	
	buffer = (char *)malloc(BLOCK_BUFFER_SIZE);
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
	section_header = (partition_section_info_t*)calloc(section_num, sizeof(partition_section_info_t));
	memset(buffer, 0x00, BLOCK_BUFFER_SIZE);
	if(read(ifd, section_header, sizeof(partition_section_info_t)*section_num) != sizeof(partition_section_info_t)*section_num)
	{
		perror("read partition error");
		goto error;
	}
	
	for(i = 0; i < section_num; ++i)
	{
		offset = 0;
		size = 0;
		current = 0;
		calc_check_sum = 0;
		readlen = BLOCK_BUFFER_SIZE;
		//printf("%d times, section_num %d**************%d\n", i, section_num,section_header[i].version);
		if(section_header[i].version == 1)  //upgrade,need to check
		{
			
			save_check_sum = section_header[i].check_sum;
			for(j = 0; j<i; ++j)
			{
				offset += section_header[j].size;
			}

			size = section_header[i].size;
			
			if(lseek(ifd, sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*section_num+offset, SEEK_SET) == -1)
			{
				perror("lseek error");
				goto error;
			}
			memset(buffer, 0x00, BLOCK_BUFFER_SIZE);
			total_len = size;
			//printf("offset %d  size %d\n", offset, size);

			
			while(size != 0)
			{
				if(size < readlen)
				{
					readlen = size;
				}
				if((read_cnt = read(ifd, buffer, readlen)) != -1)
				{
					calc_check_sum += simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE);
					//printf("************************************check:%x\n", calc_check_sum);
					current += read_cnt;
					size -= read_cnt;
					memset(buffer, 0x00, BLOCK_BUFFER_SIZE);
					printf("check complete:%f%%\n ", 100*(double)current/(double)total_len);
				}
				else
				{
					perror("read error");
					goto error;
				}

				
			}
			//printf("sava:%x  calc:%x\n", save_check_sum, calc_check_sum);
			if(save_check_sum == calc_check_sum)
			{
				printf("%d partition check sum is OK\n", i);			
			}
			else
			{
				printf("check_sum error\n");
				goto error;
			}
		}
	}

	close(ifd);
	free(buffer);
	free(section_header);
	printf("success\n");
	return 0;//successful
	
error:
	printf("error\n");
	close(ifd);
	free(buffer);
	free(section_header);
	return -1;

}



int upgrade(void)
{
	int fd = -1, ifd = -1;
	//char *upgrade_img_name = "../generate_upgrade_file/upgrade_file";
	char *upgrade_img_name = "/mnt/usb/upgrade_file";
	char *partition_name = NULL;
	struct mtd_info_user info;
	int regcount = 0;
	int res = 0, cnt = 0, write_cnt = 0;
	int imglen = 0, total_len = 0;
	int readlen = 0;
	char *write_buf = NULL;
	int percent = 0;
	int size = 0, offset = 0;
	upgrade_file_header *ufh = NULL;
	partition_section_info_t *section_header = NULL;
	memset(&info, 0x00, sizeof(info));
	int i = 0;

	//int partition_offset = 0, partition_size = 0;
	int read_size = 0;
	printf("Preparing write,checking...\n");
	
	if(check_sum_partition(upgrade_img_name) != 0)
	{
		printf("check_sum calc error\n");
		goto error;
	}

	

	ifd = open(upgrade_img_name, O_RDONLY);
	ufh = (upgrade_file_header*)malloc(sizeof(upgrade_file_header));
	read(ifd, ufh, sizeof(upgrade_file_header));
	section_header = (partition_section_info_t*)calloc(ufh->section_num, sizeof(partition_section_info_t));
	read(ifd, section_header, sizeof(partition_section_info_t)*ufh->section_num);
	//lseek(ifd, sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*ufh->section_num, SEEK_SET);


	for(i = 0; i< ufh->section_num; ++i)
	{
		percent = 0;
		int offset = 0;
		if(section_header[i].version == 1)
		{
			char dev_path_name[16] = {0};
			int j = 0;
			sprintf(dev_path_name,"/dev/%s", section_header[i].dev_name);
			printf("name:%s\n", section_header[i].dev_name);
			if((fd = open(dev_path_name, O_RDWR)) < 0)
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

			//erase
			if(regcount == 0)
			{
				if((res = non_region_erase(fd, 0, (info.size/info.erasesize), 0)) != 0)
				{
					perror("erase error\n");
				}
			}
			printf("%d partition erase down\n", i);


			for(j = 0; j<i; ++j)
			{
				offset += section_header[j].size;
			}

			//size = section_header[i].size;
			
			if(lseek(ifd, sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*ufh->section_num+offset, SEEK_SET) == -1)
			{
				perror("lseek error");
				goto error;
			}
			read_size = info.writesize;
			//write
			write_buf = (char*)malloc(info.writesize);
			if(write_buf == NULL)
			{
				perror("malloc error");
				goto error;
			}

			printf("Starting write:\n");
			total_len = section_header[i].size;
			
			while(total_len != 0)
			{
				if(total_len < info.writesize)
				{
					read_size = total_len;
				}
				memset(write_buf, 0xff, info.writesize);
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
				
				if((write_cnt = write(fd, write_buf, info.writesize)) != info.writesize)
				{
					printf("write mtd device error\n");
					goto error;
				}
				else
				{
					percent += cnt;
					printf("write mtd device Ok, complete:%f%%\n ", 100*(double)percent/(double)(section_header[i].size));
					
				}
				
				total_len -= cnt;
				
			}
			printf("\n");
			printf("%d partition Write done\n", i);
			if(write_buf != NULL)
			{
				free(write_buf);
			}
			write_buf = NULL;
		}

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
	if(ufh != NULL)
		free(ufh);
	if(section_header != NULL)
		free(section_header);
	if(write_buf != NULL)
	{
		free(write_buf);
	}
	write_buf = NULL;
	return -1;


}


static int fd = -1;
int main(int argc, char *argv[])
{

	struct sockaddr_in server_sa = {0};  
    int connect_ret = -1;  

    int send_ret = -1;  

    sighandler_t sig_ret = (sighandler_t)-1;  

    fd = socket(AF_INET,SOCK_STREAM,0);   //建立socket连接，设置连接类型  
    if(fd < 0)  
    {  
            perror("socket");  
            exit(-1);  
    }  
    printf("fd:%d\n",fd);  

    sig_ret = signal(SIGPIPE,sigpipe_handle);  //为sigpipe信号绑定处理函数  
    if(SIG_ERR == sig_ret)  
    {  
            perror("signal");  
            exit(-1);  
    }  

    server_sa.sin_port = htons(SERVER_PORT);  
    server_sa.sin_addr.s_addr = inet_addr(SERVER_IP);  
    server_sa.sin_family = AF_INET;  
    connect_ret = connect(fd,(struct sockaddr *)&server_sa,sizeof(server_sa));  //向服务器发送连接请求  
    if(connect_ret < 0)  
    {  
            perror("connect");  
            exit(-1);  
    }  
    debug("connect done\n");  

    char sendbuf[20] = {0};  
    int recv_cnt = 0;
    while(1)    //将用户输入的数据发送给服务器端，当输入end时，客户端退出  
    {  
            printf("input your data\n");  
            scanf("%s",sendbuf);  
            if(!strncmp(sendbuf, "end",3))  
            {  
                    debug("exiting....\n");  
                    close(fd);  
                    return 0;  
            }  

            send_ret = send(fd,sendbuf,strlen(sendbuf),0);  //向服务器发送信息  
            if(send_ret < 0)  
            {  
                    perror("send");  
                    exit(-1);  
            }  
            printf("data sent successfully:%d\n",send_ret);
            memset(sendbuf, 0x00, sizeof(sendbuf));
            recv_cnt = recv(fd, sendbuf, sizeof(sendbuf), 0);
            if(recv_cnt < 0)
            {
            	perror("recv error");
            	close(fd);
            	exit(-1);
            }
            //sendbuf[recv_cnt] = '\0';
            printf("recv : %s\n", sendbuf);
    }  
}  
  
void sigpipe_handle(int arg)  //打印提示信息后再退出  
{  
    if(SIGPIPE == arg)  
    {  
            printf("server disconnect\n");  
            close(fd);  
            debug("exiting\n");  
            exit(1);  
    }  
}  