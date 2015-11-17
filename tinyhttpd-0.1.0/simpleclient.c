/*********************************************************************
 * Function: 测试OPTIONS、HEAD、GET、POST指令
 * Filename: simpleclient.c
 * Author:  zhangshifa 
 * Date: 2015年11月17日 17:58:40
********************************************************************/

/** （1）使用simpleclient测试HEAD指令，使用./simpleclient head。
 *  （2）使用simpleclient测试OPTIONS指令，使用./simpleclient options
 *  
 *  （3）使用一下方法对比测试结果：
 *  
 *  测试 HEAD指令
 *  1.curl --head ip:11000 获得http的head。
 *  2.使用telnet发送HEAD请求:telnet ip 11000,测试HEAD指令。
 *  
 *  测试 OPTIONS指令
 *  使用telnet发送OPTIONS请求：
 *  ==>telnet ip 11000
 *  ==>ctrl+]
 *  ==>enter回车
 *  ==>输入OPTIONS / HTTP/1.1
 *  ==>enter enter 回车两下
 *  使用telnet方法可以测试GET、POST、OPTIONS和HEAD指令
 *  
 *   */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int sockfd;
	int len;
	struct sockaddr_in address;
	int result;
	char buf[1024] = {0};

	/** 接受数据缓存 */
	char get[1024] = {0};

	if (argc < 2)
	{
		fprintf(stderr,"\n\033[31musage:\033[34m%s\033[32m %s|%s|%s|%s\033[0m\t\n\n",argv[0], "get", "post", "head", "options");
		exit(1);
	}
        /** 至少两个'\n'作为结束 */
	if (strcmp(argv[1], "get") == 0)
	{
		char get_buf[1024] = "GET / HTTP/1.1\n\n\n";
		strcpy(buf, get_buf);
	}
	else if (strcmp(argv[1], "post") == 0)
	{       /** 中间使用'\n'进行分隔 */
		char post_buf[1024] = "POST / HTTP/1.1\nContent-Length: 12 \n\n";
		strcpy(buf, post_buf);
	}
	else if (strcmp(argv[1], "head") == 0)
	{
		char head_buf[1024] = "HEAD / HTTP/1.1\n\n\n\n";
		strcpy(buf, head_buf);
	}
	else if (strcmp(argv[1], "options") == 0)
	{
		char options_buf[1024] = "OPTIONS / HTTP/1.1\n\n\n";
		strcpy(buf, options_buf);
	}
	else
	{
		fprintf(stderr, "\n\033[31m[%s] is not legal command or has not implemented yet.\033[0m\n\n", argv[1]);
		fprintf(stderr,"\n\033[31musage:\033[34m%s\033[32m %s|%s|%s|%s\033[0m\t\n\n",argv[0], "get", "post", "head", "options");
		exit(1);
	}

	/** 建立tcp连接 */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");/** 本机IP地址*/
	address.sin_port = htons(11000);/** tinyhttpd的端口 */
	len = sizeof(address);

	/** 建立连接 */
	result = connect(sockfd, (struct sockaddr *)&address, len);
	if (result == -1)
	{
		perror("oops:client:");
		return -1;
	}

	/** 发送http请求 */
	write(sockfd,buf, sizeof(buf));
	/** 接受回应 */
	read(sockfd, get, sizeof(get));

	/**打印返回数据 */
	printf("-----------------------------show buffer -----------------------------\n");
	printf("%s",get);
	printf("----------------------------------------------------------------------\n");
	/** 断开连接 */
	close(sockfd);

        return 0;
}


