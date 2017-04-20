#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <termios.h>
                                   
#define FALSE -1
#define TRUE 0

#define COMM_NOPARITY 0             /*无校验位、奇校验、偶校验*/
#define COMM_ODDPARITY 1            
#define COMM_EVENPARITY 2           
             
#define COMM_ONESTOPBIT 1     /*停止位*/
#define COMM_TWOSTOPBITS 2   


typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef signed int LONG;
typedef unsigned int DWORD;

typedef struct com_attr       /*串口属性结构*/         
{
	unsigned int baudrate;    /*波特率*/
	unsigned char databits;    /*数据位*/
	unsigned char stopbits;    /*停止位*/
	unsigned char parity;    /*校验位*/
}com_attr;


int com_open(const char *dev)
{
	int fd = -1;
	fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1)
	{
		perror("串口打开失败!");
		exit(-1);
	}
	if(fcntl(fd, F_SETFL, 0) == -1)    /*设置以阻塞模式打开*/
	{
		perror("error fcntl");
		exit(-1);
	}
	return fd;
}


/**************************************************
 *函数名：set_com_attr()
 *功  能：设置串口属性
 *参  数：int fd,属性结构体com_arrt *attr
 *返回值：void
**************************************************/
int set_com_attr(int fd, com_attr *attr)
{
	struct termios opt;
//	com_attr *attr = pattr;
	memset(&opt, 0, sizeof(struct termios));
	tcgetattr(fd, &opt);
	cfmakeraw(&opt);    
	
	
	/*******************波特率********************/
	printf("set baudrate %d\n", attr->baudrate);
	switch (attr->baudrate)
	{
		case 50:
			cfsetispeed(&opt, B50);
			cfsetospeed(&opt, B50);
			break;
		case 75:
			cfsetispeed(&opt, B75);
			cfsetospeed(&opt, B75);
			break;
		case 110:
			cfsetispeed(&opt, B110);
			cfsetospeed(&opt, B110);
			break;
		case 134:
			cfsetispeed(&opt, B134);
			cfsetospeed(&opt, B134);
			break;
		case 150:
			cfsetispeed(&opt, B150);
			cfsetospeed(&opt, B150);
			break;
		case 200:
			cfsetispeed(&opt, B200);
			cfsetospeed(&opt, B200);
			break;
		case 300:
			cfsetispeed(&opt, B300);
			cfsetospeed(&opt, B300);
			break;
		case 600:
			cfsetispeed(&opt, B600);
			cfsetospeed(&opt, B600);
			break;
		case 1200:
			cfsetispeed(&opt, B1200);
			cfsetospeed(&opt, B1200);
			break;
		case 1800:
			cfsetispeed(&opt, B1800);
			cfsetospeed(&opt, B1800);
			break;
		case 2400:
			cfsetispeed(&opt, B2400);
			cfsetospeed(&opt, B2400);
			break;
		case 4800:
			cfsetispeed(&opt, B4800);
			cfsetospeed(&opt, B4800);
			break;
		case 9600:
			cfsetispeed(&opt, B9600);
			cfsetospeed(&opt, B9600);
			break;
		case 19200:
			cfsetispeed(&opt, B19200);
			cfsetospeed(&opt, B19200);
			break;
		case 38400:
			cfsetispeed(&opt, B38400);
			cfsetospeed(&opt, B38400);
			break;
		case 57600:
			cfsetispeed(&opt, B57600);
			cfsetospeed(&opt, B57600);
			break;
		case 115200:
			cfsetispeed(&opt, B115200);
			cfsetospeed(&opt, B115200);
			break;
		case 230400:
			cfsetispeed(&opt, B230400);
			cfsetospeed(&opt, B230400);
			break;
		case 460800:
			cfsetispeed(&opt, B460800);
			cfsetospeed(&opt, B460800);
			break;
		case 500000:
			cfsetispeed(&opt, B500000);
			cfsetospeed(&opt, B500000);
			break;
		case 576000:
			cfsetispeed(&opt, B576000);
			cfsetospeed(&opt, B576000);
			break;
		case 921600:
			cfsetispeed(&opt, B921600);
			cfsetospeed(&opt, B921600);
			break;
		case 1000000:
			cfsetispeed(&opt, B1000000);
			cfsetospeed(&opt, B1000000);
			break;
		case 1152000:
			cfsetispeed(&opt, B1152000);
			cfsetospeed(&opt, B1152000);
			break;
		case 1500000:
			cfsetispeed(&opt, B1500000);
			cfsetospeed(&opt, B1500000);
			break;
		case 2000000:
			cfsetispeed(&opt, B2000000);
			cfsetospeed(&opt, B2000000);
			break;
		case 2500000:
			cfsetispeed(&opt, B2500000);
			cfsetospeed(&opt, B2500000);
			break;
		case 3000000:
			cfsetispeed(&opt, B3000000);
			cfsetospeed(&opt, B3000000);
			break;
		case 3500000:
			cfsetispeed(&opt, B3500000);
			cfsetospeed(&opt, B3500000);
			break;
		case 4000000:
			cfsetispeed(&opt, B4000000);
			cfsetospeed(&opt, B4000000);
			break;
		default:
			printf("unsupported baudrate %d\n", attr->baudrate);
			return FALSE;
			break;
	}


	opt.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	opt.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
	opt.c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
	opt.c_cflag |=  CS8;      /* Set the data bits = 8                                 */
	
	opt.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	opt.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */ 
		
		
	opt.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
	opt.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

	opt.c_oflag &= ~OPOST;/*No Output Processing*/

	
	
	tcflush(fd, TCIOFLUSH);	       //刷清缓冲区
	if (tcsetattr(fd, TCSANOW, &opt) < 0)
	{
		printf("tcsetattr faild\n");
		return FALSE;
	}
	return TRUE;
}


/**************************************************
 *函数名：get_com_attr()
 *功  能：获取串口属性
 *参  数：int fd
 *返回值：void
**************************************************/
void get_com_attr(int fd)
{
	struct termios opt;
	
	memset(&opt, 0, sizeof(struct termios));
	tcgetattr(fd, &opt);
	cfmakeraw(&opt);
} 

/**************************************************
 *函数名：com_read()
 *功  能：读串口操作
 *参  数：int fd，unsigned char *rbuff, unsigned int nbytes
 *返回值：int ret读取字节数
**************************************************/
int com_read(int fd, BYTE *read_buff, DWORD nbytes)
{
	int ret;
	if(fd < 0)
	{
		printf("com_read error");
		exit(-1);
	}
	ret = read(fd, read_buff, nbytes);
	return ret;
}

/**************************************************
 *函数名：com_write()
 *功  能：写串口操作
 *参  数：int fd，unsigned char *wbuff, unsigned int nbytes
 *返回值：int ret写入字节数
**************************************************/
int com_write(int fd, BYTE *write_buff, DWORD nbytes)
{
	int ret;
	if(fd < 0)
	{
		printf("com_write error");
		exit(0);
	}
	ret = write(fd, write_buff, nbytes);
	return ret;
}

/**************************************************
 *函数名：com_close()
 *功  能：关闭串口
 *参  数：fd
 *返回值：void
**************************************************/
void com_close(int fd)
{
	if(fd < 0)
	{
		printf("com_close error");
		exit(0);
	}
	close(fd);
}

void com_read_baudrate(int fd)
{
	struct termios opt;
	
	memset(&opt, 0, sizeof(struct termios));
	tcgetattr(fd, &opt);
	speed_t i_speed = cfgetispeed(&opt);
	speed_t o_speed = cfgetospeed(&opt);
	if(i_speed == B115200)
	{
		printf("115200\n");

	}
	else if(i_speed == B9600)
	{
		printf("9600\n");
	}

}
int com_init(void)
{
	const char *dev = "/dev/ttyS0";
	int fd = com_open(dev);

	com_attr attr = {0};
	attr.baudrate = 9600;
	attr.databits = 8;
	attr.parity = COMM_NOPARITY;
	attr.stopbits = COMM_ONESTOPBIT;
	if(set_com_attr(fd, &attr) != 0)
		{
			perror("set com_rfid attr failed\n");
			exit(-2);
		}

	return fd;
}

#define MAX_SIZE 512

#define HASH_SCALE 7
#define UPGRADE_STR "upgrade"
//#define BLOCK_BUFFER_SIZE (128*1024)
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



int read_block(int fd, char *buffer, int block_size)
{
	int per_read = 0;
	char tmp_buf[MAX_SIZE] = {0};
	int current = 0;
	while(block_size >= 0)
		{
			memset(tmp_buf, 0x00, MAX_SIZE);
			if((per_read = read(fd, tmp_buf, MAX_SIZE)) == -1)
				{
					perror("read error");
					return -1;
				}
			memcpy(buffer+current, tmp_buf, per_read);
			current += per_read;
			if(block_size <= per_read)
				{
					return current;//real block length
				}
			else
				{
					block_size -= per_read;
				}
			
		}
	return -1;
	
}

int handle_rdwr(int fd, int log_fd)
{
	char read_buffer[MAX_SIZE+4] = {0};
	int read_len = 0;
	unsigned int check_sum = 0;
	
	memset(read_buffer, 0x00, MAX_SIZE+4);
	write(fd, "go", sizeof("go"));
	read_len = read_block(fd, read_buffer, MAX_SIZE+4);
	check_sum = *(unsigned int*)&read_buffer[MAX_SIZE];
	
	if( read_len == -1 || check_sum != simple_hash(read_buffer,MAX_SIZE,HASH_SCALE))
		{
			tcflush(fd, TCIOFLUSH);
			write(fd, "re", sizeof("re"));//again
		}
	else
		{
			write(fd, "ok", sizeof("ok"));
			write(log_fd, read_buffer, read_len);
		}

	return 0;

}

/*
int recv_header(int fd)
{
	char read_buffer[MAX_SIZE+4] = {0};
	int read_len = 0;
	memset(read_buffer, 0x00, MAX_SIZE+4);

	
}
*/
int main(int argc, char const *argv[])
{
	/* code */
	char buffer[MAX_SIZE] = {0};
	char *log = "/tmp/log";
	int read_len = 0, write_len = 0;
	int fd = com_init();
	int log_fd = open(log, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	tcflush(fd, TCIOFLUSH);
	com_read_baudrate(fd);
	/*while(1)
		{
			handle_rdwr(fd, log_fd);
		}
		*/
	
	while(1)
	{
		memset(buffer, 0x00, MAX_SIZE);
		if((read_len = com_read(fd, buffer, MAX_SIZE)) == -1)
		{
			perror("read error");
			exit(-1);
		}
		//printf("readlen:%d ", read_len);
		read_len = read_len > MAX_SIZE ? MAX_SIZE : read_len;
		if((write_len = write(log_fd, buffer, read_len)) == -1)
		{
			perror("write_error");
			exit(-1);
		}
		//printf("write_len:%d \n", write_len);
	}
	
	return 0;
}

