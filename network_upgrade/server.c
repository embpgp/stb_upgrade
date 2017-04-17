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
#define BLOCK_BUFFER_SIZE (128*1024)
#define BLOCK_CHECK_SUM_SIZE 4

#define TATOL_SIZE (BLOCK_BUFFER_SIZE+BLOCK_CHECK_SUM_SIZE)
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
  
void *func(void *fd) //线程处理函数  
{  
    int *accept_ret = (int *)fd;  
    char *buffer = NULL;  
    int recv_ret = -1; 
    int fd = -1; 
    char *fname = UPGRADE_STR;
    if((fd = open(fname, O_RDONLY)) == -1)
    {
    	perror("open target file error");
    	exit(-1);
    }
    buffer = (char*)malloc(TATOL_SIZE);//buffer size + per package check_sum
    memset(buffer, 0x00, TATOL_SIZE)
    while(1)  
    {  
            recv_ret = recv(*accept_ret,buffer,TATOL_SIZE,0);//程序先阻塞在这里等待客户端发的指令 
            if(recv_ret < 0)  
            {  
                    perror("recv");  
                    free(buffer);
                    close(fd);
                    exit(-1);  
            }   
            else if(recv_ret == 0)  //连接断开  
            {  
                   
                    printf("close:%d,exiting subthread\n",*accept_ret);  
                    free(accept_ret);  //释放动态分配的描述符空间  
                    close(*accept_ret) //关闭读取描述符  
                    free(buffer);
                    close(fd);
                    pthread_exit(NULL);//线程返回  
            } 

            //continue send
            if(strncmp(buffer, "OK", recv_ret) == 0)
            {
            	memset(buffer, 0x00, TATOL_SIZE);
            	if(read(fd, buffer, BLOCK_BUFFER_SIZE) == -1)
            	{
            		perror("read file error");
            		free(accept_ret);  //释放动态分配的描述符空间  
                    close(*accept_ret) //关闭读取描述符  
                    free(buffer);
                    close(fd);
                    pthread_exit(NULL);//线程返回 

            	}
            	else
            	{
            		*(unsigned int*)&buffer[BLOCK_BUFFER_SIZE] = check_sum(buffer, BLOCK_BUFFER_SIZE, HASH_SCALE);  //check_sum
            		if(send(*accept_ret, buffer, TATOL_SIZE, 0) == -1)
            		{
            			perror("read file error");
            			free(accept_ret);  //释放动态分配的描述符空间  
                   		close(*accept_ret) //关闭读取描述符  
                  	    free(buffer);
                        close(fd);
                        pthread_exit(NULL);//线程返回 

            		}
            	}
            }//resend
            else if(strncmp(buffer, "error", recv_ret) == 0)
            {

            }
            send(*accept_ret, receivebuf, recv_ret, 0); 
            printf("%d bytes from %d,%s\n",recv_ret,*accept_ret,receivebuf);  
            memset(receivebuf,0,strlen(receivebuf));  
    }  
}  
  
void sigint_handle(int sig) //信号处理函数  
{  
    if(SIGINT != sig)return;  
    printf("\nserver termated\n");  
    close(fd);  
    exit(1);  
}  