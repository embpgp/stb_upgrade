#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>

#define DEBUG  
#ifdef DEBUG  
#define debug(x) printf(x)  
#else  
#define debug(x)  
#endif  

#define HASH_SCALE 7
#define UPGRADE_STR "upgrade_file"
#define BLOCK_BUFFER_SIZE (1024)
#define BLOCK_CHECK_SUM_SIZE 4
#define TATOL_SIZE (BLOCK_BUFFER_SIZE+BLOCK_CHECK_SUM_SIZE)
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
		sum += (((*p) >> scale) | ((*p) << (sizeof(unsigned int)*8-scale)));
		size -= sizeof(unsigned int*);
		p++;
		
	}
	return sum;
}

#define SERVER_PORT 8888  
#define SERVER_IP "192.168.10.82"  
#define LISTEN_NUM 10  
  
typedef void (*sighandler_t)(int);  
  
void sigint_handle(int sig);   //sigint信号处理函数，Ctrl + c所发送的信号  
void *func(void *arg);  //线程处理函数  
  
 
static int fd = -1; 
int main(int argc , char * argv[])  
{  
	 
    int bind_ret = -1;  
    int on = 0;
    struct sockaddr_in sa_in = {0};  
    int listen_ret = -1;  
    int *accept_ret = NULL;  
    struct sockaddr_in client_sa = {0};  
    socklen_t client_len = 0;  

    pthread_t thread = -1;  
    int ret = -1;  

    sighandler_t sig_ret = (sighandler_t)-1;  
    sig_ret = signal(SIGINT,sigint_handle); //设置sigint信号的处理函数  
    if(SIG_ERR == sig_ret)  
    {  
            perror("signal");  
            exit(-1);  
    }  

    fd = socket(AF_INET,SOCK_STREAM,0);  //建立一个监听socket，明确了socket的类型和类别：ipv4和tcp连接  
    if(fd < 0)  
    {  
            perror("soket");  
            exit(-1);  
    }  
    printf("fd:%d\n",fd);  
    on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) ); //使得端口立即可用
    sa_in.sin_family = AF_INET;  
    sa_in.sin_port = htons(SERVER_PORT); //将端口转为网络序，赋给结构体  
    sa_in.sin_addr.s_addr = inet_addr(SERVER_IP);  //将点分十进制ip地址转换为二进制  
    bind_ret = bind(fd,(const struct sockaddr *)&sa_in,sizeof(sa_in));  //为监听描述符绑定ip和port，也被称为命名  
    if(bind_ret < 0)  
    {  
            perror("bind");  
            exit(-1);  
    }  
    debug("bind success\n");  

    listen_ret = listen(fd,LISTEN_NUM);  //设置socket为监听状态  
    if(listen_ret < 0)  
    {  
            perror("listen");  
            exit(-1);  
    }  
    debug("listening\n");  
    while(1)  //在循环中处理连接请求  
    {  
            accept_ret = (int *)malloc(sizeof(int));  //动态分配的原因：避免读写描述符被多个线程混用
            *accept_ret = accept(fd,(struct sockaddr *)&client_sa,&client_len);  //从链接队列中获取建立好的socket连接  
            if(accept_ret < 0)  
            {  
                    perror("accept");  
                    exit(-1);  
            }  
            printf("accept_ret,fd for client:%d\n",*accept_ret);  

            ret = pthread_create(&thread,NULL,func,(void *)accept_ret);  //为连接创建线程  
            if(ret != 0)  
            {  
                    perror("pthread_create");  
                    exit(-1);  
            }  
    }  

    return 0;  
}  
  
//不至于看起来太臃肿
#define EXCEPTION_EXIT do\
{\
	free(accept_ret);  \
    close(*accept_ret); \
    free(buffer);\
    free(ufh);\
    free(section_header);\
    close(ifd);\
    pthread_exit(NULL);\
}while(0);

void *func(void *fd) //线程处理函数  
{  
    int *accept_ret = (int *)fd;  
    char *buffer = NULL;  
    int recv_ret = -1; 
    int ifd = -1; 
    char *fname = UPGRADE_STR;
    upgrade_file_header *ufh = NULL;
    partition_section_info_t *section_header = NULL;
    if((ifd = open(fname, O_RDONLY)) == -1)
    {
    	perror("open target file error");
    	exit(-1);
    }
    buffer = (char*)malloc(TATOL_SIZE);//buffer size + per package check_sum
    if(buffer == NULL)
    {
    	perror("malloc error");
    	exit(-1);
    }
    memset(buffer, 0x00, TATOL_SIZE);
    ufh = malloc(sizeof(*ufh));
    memset(ufh, 0x00, sizeof(*ufh));
    
    
    
	if((recv_ret = recv(*accept_ret, buffer, TATOL_SIZE, 0) == -1))
	{
		perror("recv error");
		EXCEPTION_EXIT

	}
	printf("CMD:%s\n", buffer);
	if(strncmp(buffer, "start", sizeof("start")) != 0)
	{
		printf("error");
		EXCEPTION_EXIT
	}	
	if(read(ifd, buffer, sizeof(upgrade_file_header)) == -1)
	{
		perror("read file header error");
		EXCEPTION_EXIT

	}
	else
	{
		memcpy(ufh, buffer, sizeof(*ufh));
		read(ifd, buffer+sizeof(*ufh), ufh->section_num*sizeof(partition_section_info_t));
		section_header = malloc(ufh->section_num*sizeof(partition_section_info_t));
		memcpy(section_header, buffer+sizeof(*ufh), ufh->section_num*sizeof(partition_section_info_t));
		*(unsigned int*)&buffer[BLOCK_BUFFER_SIZE] = simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE);  //check_sum
		if(send(*accept_ret, buffer, TATOL_SIZE, 0) == -1)
		{
			perror("send  error");
			EXCEPTION_EXIT

		}
	}
    for(int i = 0; i< ufh->section_num; ++i)
    {  
    	int offset = 0;
		int size = 0;
		int read_len = BLOCK_BUFFER_SIZE;
		int current = 0;
		//calc_check_sum = 0;
		int j = 0;
		int flag = 1;
    	if(section_header[i].version == 1)
    	{
    		for(j = 0; j<i; ++j)
			{
				offset += section_header[j].size;
			}

			size = section_header[i].size;
			lseek(ifd, sizeof(upgrade_file_header)+sizeof(partition_section_info_t)*ufh->section_num+offset, SEEK_SET);
			while(size || flag)
			{
				if(size < BLOCK_BUFFER_SIZE)
				{
					read_len = size;
				}
	            recv_ret = recv(*accept_ret,buffer,TATOL_SIZE,0);//程序先阻塞在这里等待客户端发的指令 
	            if(recv_ret < 0)  
	            {  
	                    perror("recv");  
	                    EXCEPTION_EXIT  
	            }   
	            else if(recv_ret == 0)  //连接断开  
	            {  
	                   
	                    printf("close:%d,exiting subthread\n",*accept_ret);  
	                    EXCEPTION_EXIT
	                    
	            } 
	            //continue send
	            printf("CMD:%s\n", buffer);
	            if(strncmp(buffer, "continue", sizeof("continue")) == 0)
	            {
	            	memset(buffer, 0x00, TATOL_SIZE);
	            	if((read(ifd, buffer, read_len)) == -1)
	            	{
	            		perror("read error");
	            		EXCEPTION_EXIT
	            	}
	            	else
	            	{
	            		*(unsigned int*)&buffer[BLOCK_BUFFER_SIZE] = simple_hash(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE);  //check_sum
	            		if(send(*accept_ret, buffer, TATOL_SIZE, 0) == -1)
	            		{
	            			perror("send error");
	            			EXCEPTION_EXIT
	            		}
	            		size -= read_len;
	            		current += read_len;
	            		printf("complete:%f%%\n", 100*(double)current/(double)section_header[i].size);
	            	}

	            }

	            //resend
	            else if(strncmp(buffer, "error", sizeof("error")) == 0)
	            {
	            	if(send(*accept_ret, buffer, TATOL_SIZE, 0) == -1)
	            		{
	            			perror("send error");
	            			EXCEPTION_EXIT
	            		}

	            }
	            else if(strncmp(buffer, "complete", sizeof("complete")) == 0)
	            {
	            	flag = 0;
	            }
        	}
             
        } 
    }  
}  
  
void sigint_handle(int sig) //信号处理函数  
{  
    if(SIGINT != sig)return;  
    printf("\nserver termated\n");  
    close(fd);  
    exit(1);  
}  