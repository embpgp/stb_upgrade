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

#define BLOCK_BUFFER_SIZE (1024)
#define BLOCK_CHECK_SUM_SIZE 4
#define TATOL_SIZE (BLOCK_BUFFER_SIZE+BLOCK_CHECK_SUM_SIZE)


#define DEBUG  
#ifdef DEBUG  
#define debug(x) printf(x)  
#else  
#define debug(x)  
#endif  

#define SERVER_PORT 8888  
#define SERVER_IP "192.168.10.82"  
#define LISTEN_NUM 10  

#define AUPBND (sizeof(unsigned int) - 1)
#define BND(num) (((num) + (AUPBND)) & (~(AUPBND)))

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
		p++;
		
	}
	return sum;
}




int upgrade_partition(char *dev_name, char *data, unsigned int partition_size)
{
	int fd = -1;
	struct mtd_info_user info;
	int regcount = 0,res = 0;
	int write_cnt = 0;
	int total_len = 0;
	char *write_buf = NULL;
	int percent = 0;
	int size = 0;
	int read_size = 0;
	char dev_path_name[16] = {0};
	sprintf(dev_path_name,"/dev/%s", dev_name);
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

	read_size = info.writesize;
	//write
	write_buf = (char*)malloc(info.writesize);
	if(write_buf == NULL)
	{
		perror("malloc error");
		goto error;
	}

	printf("Starting write:\n");
	total_len = partition_size;
	
	while(total_len != 0)
	{
		if(total_len < info.writesize)
		{
			read_size = total_len;
		}
		memset(write_buf, 0xff, info.writesize);
		memcpy(write_buf, data+percent, read_size);

		if((write_cnt = write(fd, write_buf, info.writesize)) != info.writesize)
		{
			printf("write mtd device error\n");
			goto error;
		}
		else
		{
			percent += read_size;
			printf("write mtd device Ok, complete:%f%%\n ", 100*(double)percent/(double)(partition_size));
			
		}
		
		total_len -= read_size;
		
	}
	printf("\n");

error:
	if(fd != -1)
	{
		close(fd);
	}
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
    int i = 0;
    sighandler_t sig_ret = (sighandler_t)-1;  
    sleep(5);   //wait the network ...
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

    char *buffer = (char*)malloc(TATOL_SIZE); 
    upgrade_file_header *ufh = malloc(sizeof(upgrade_file_header));
    memset(ufh, 0x00, sizeof(upgrade_file_header));
    

    int recv_cnt = 0;
    printf("begin.....\n");
    send_ret = send(fd,"start",sizeof("start"),0);  //向服务器发送信息  
    if(send_ret < 0)  
    {  
            perror("send");  
            exit(-1);  
    }  
    if((recv(fd, buffer, TATOL_SIZE, 0)) == -1)
    {
    	perror("recv");
    	exit(-1);
    }
    printf("header recv OK\n");
    memcpy(ufh, buffer, sizeof(*ufh));
    partition_section_info_t *section_header = malloc(sizeof(partition_section_info_t)*ufh->section_num);
    memset(section_header, 0x00, sizeof(partition_section_info_t)*ufh->section_num);
    memcpy(section_header, buffer+sizeof(*ufh), sizeof(partition_section_info_t)*ufh->section_num);
    unsigned head_check = *(unsigned int*)&buffer[BLOCK_BUFFER_SIZE];
    if(head_check != simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE))
    {
    	printf("head check error\n");
    	exit(-1);
    }
    for(i = 0; i<ufh->section_num; ++i)  
    {  
    	int offset = 0;
		int size = 0;
		int current = 0;
		int j = 0;
		int read_len = BLOCK_BUFFER_SIZE;
		unsigned int partition_check_sum = 0;
        if(section_header[i].version == 1)
        {

        	for(j = 0; j<i; ++j)
			{
				offset += section_header[j].size;
			}

			size = section_header[i].size;

			unsigned int align_value = BND(size);
			printf("raw size:0x%x, bnd size:0x%x\n", size, align_value);
			char *section_buffer = malloc(align_value);
			if(section_header == NULL)
			{
				perror("malloc error");
				exit(-1);
			}
			int flag = 1;
			memset(section_buffer, 0x00, align_value);
			while(size)
			{
				int check_per_pkg = 0;
				
				if(size < BLOCK_BUFFER_SIZE)
				{
					read_len = size;
				}
				memset(buffer, 0x00, TATOL_SIZE);
				if(flag == 1)
				{
					send(fd, "continue", sizeof("continue"), 0);
				}
				
				if(recv(fd, buffer, TATOL_SIZE, 0) == -1)
				{
					perror("recv error");
					exit(-1);
				}
				check_per_pkg = *(unsigned int *)&buffer[BLOCK_BUFFER_SIZE];
				if(simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE) != check_per_pkg) //again
				{
					send(fd, "error", sizeof("error"), 0);
					flag = 0;
					continue;
				}
				//OK
				size -= read_len;
				flag = 1;
				
				memcpy(section_buffer+current, buffer, read_len);
				current += read_len;
				printf("complete:%f%%\n", 100*(double)current/(double)section_header[i].size);

			}
			send(fd, "complete", sizeof("complete"), 0);
			partition_check_sum = simple_hash(section_buffer, align_value, HASH_SCALE);
			printf("calc:0x%x,raw:0x%x\n", partition_check_sum, section_header[i].check_sum);
			if(partition_check_sum == section_header[i].check_sum)
			{
				printf("partition_check_sum is OK\n");

			}
			else
			{
				printf("partition_check_sum is error");
				exit(-1);
			}
			upgrade_partition(section_header[i].dev_name, section_buffer, section_header[i].size);
			free(section_buffer);
			//calc sec check_sum

        } 
           
    }  
    free(ufh);
    free(section_header);
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