/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */

/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
//#include <pthread.h> //Comment out the #include <pthread.h> line.
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int  get_line(int, char *, int);
void headers(int, const char *); /** header ������ */
void not_found(int);
void serve_file(int, const char *);
int  startup(u_short *);
void unimplemented(int);
void options(int);/** options ������ */

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
// ��������
/**********************************************************************/
void accept_request(int client)
{
	char buf[1024];
	int numchars;
	char method[255];
	char url[255];
	char path[512];
	size_t i, j;
	struct stat st;
	int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
	char *query_string = NULL;

	int head = 0;/** head ���� */
	int option  = 0; /** options ���� */

//      ��ȡ�����ĵĵ�һ�У���Ϊ������
	numchars = get_line(client, buf, sizeof(buf));
	i = 0;
	j = 0;
//      ��ȡ��һ������ ��Ϊmethod��������method��GET��POST��HEAD��
	while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
	{
		method[i] = buf[j];
		i++;
		j++;
	}
	method[i] = '\0';

	/*      ��ʾ���ܵ�����
	        printf("\n\033[32m=======\033[32m%s=======\033[0m\n", method);
	*/
	/** ʵ��GET,POST,HEAD,OPTIONS */
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST")
	        && strcasecmp(method, "HEAD")&&  strcasecmp(method, "OPTIONS"))
	{
		unimplemented(client);
		return;
	}
	if (strcasecmp(method, "POST") == 0)
	{
		cgi = 1;
	}
	if (strcasecmp(method, "HEAD") == 0)
	{
		head = 1;
	}
	if (strcasecmp(method, "OPTIONS") == 0)
	{
		option = 1;
	}

	// ��ȡurl
	i = 0;
	while (ISspace(buf[j]) && (j < sizeof(buf)))
		j++;
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
	{
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';

	// GET����������ʺţ���query_stringָ��'?'�������
	// û���ʺţ���query_stringΪNULL
	if (strcasecmp(method, "GET") == 0)
	{
		query_string = url;
		while ((*query_string != '?') && (*query_string != '\0'))
			query_string++;
		// ���ʺţ���Ϊ��̬����
		if (*query_string == '?')
		{
			cgi = 1;
			*query_string = '\0';
			query_string++;
		}
	}

	sprintf(path, "htdocs%s", url); // ���ݴ洢��htdocsĿ¼��
	if (path[strlen(path) - 1] == '/') // '/'����Ĭ�ϵ�home page
		strcat(path, "index.html");

//      ���stat���ش��󣬻ظ��ͻ���not_found
	if (stat(path, &st) == -1)
	{
		// ��ȡ������ headers����һ��������֮���������ͷ��
		while ((numchars > 0) && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf));
		not_found(client);
	}
	else
	{
		// �����Ŀ¼��������Ĭ�ϵ�home page
		if ((st.st_mode & S_IFMT) == S_IFDIR)
			strcat(path, "/index.html");
		// ���owner group other�κ�һ����ִ��Ȩ��
		if ((st.st_mode & S_IXUSR) ||
		        (st.st_mode & S_IXGRP) ||
		        (st.st_mode & S_IXOTH) )
		{
			cgi = 1;
		}
		if (!cgi && !head && !option)
		{
			serve_file(client, path); // ��̬����ֱ�Ӵ�Ӳ�̶�ȡ�ļ����ͳ�ȥ
		}
		if (cgi && !head  && !option)
		{
			execute_cgi(client, path, method, query_string); // ��̬����ִ��cgi�ű�
		}
		if (head)
		{
			headers(client, path);
		}
		if (option)
		{
			options(client);
		}
	}

	close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
// ֪ͨ client �Ǹ����������
/**********************************************************************/
void bad_request(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "<P>Your browser sent a bad request, ");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "such as a POST without a Content-Length.\r\n");
	send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
// ���ļ����������ݷ��͵�һ��socket�� ����̬����
/**********************************************************************/
void cat(int client, FILE *resource)
{
	char buf[1024];
	fgets(buf, sizeof(buf), resource);
	while (!feof(resource))
	{
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), resource);
	}

}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
// ֪ͨ client , CGI�ű��޷�����
/**********************************************************************/
void cannot_execute(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
// ��ӡһ��������Ϣ�����˳��ó���
/**********************************************************************/
void error_die(const char *sc)
{
	perror(sc);
	exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
// ����һ�� CGI �ű�.  ��Ҫ���û�������
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars = 1;
	int content_length = -1;

	buf[0] = 'A';
	buf[1] = '\0';
	if (strcasecmp(method, "GET") == 0)
	{
		// ��ȡ������ headers
		while ((numchars > 0) && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf));
	}
	else     /* POST */
	{
		numchars = get_line(client, buf, sizeof(buf)); // ��ȡheaders
		// һ���еض�ȡ��ֱ������content_length
		while ((numchars > 0) && strcmp("\n", buf))
		{
			buf[15] = '\0';
			if (strcasecmp(buf, "Content-Length:") == 0)
				content_length = atoi(&(buf[16]));
			numchars = get_line(client, buf, sizeof(buf));
		}
		if (content_length == -1)
		{
			bad_request(client);
			return;
		}
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);

	// �ؼ��Ĳ��֣������̺��ӽ���ͨ���ܵ�ͨ��
	// �ӽ��̸����̹ܵ�����ͼ P��ʾparent, C��ʾchild
	/*
	send to brower                                            data
	    <<<----         ---------<--<--<-----------         -----<<<
	           \        |                         |        /
	            \       |                         |       /
	           (P)   fd 1 (C)                  fd 0 (C)  (P)
	            |       |                         |       |
	            ^       v                         ^       v
	            ^       v                         ^       v
	            |       |                         |       |
	cgi_output [0]     [1]             cgi_input [0]     [1]
	            |       |                         |       |
	            |---<---|                         |---<---|
	              pipe                              pipe
	*/
	// �����ܵ�
	if (pipe(cgi_output) < 0)
	{
		cannot_execute(client);
		return;
	}
	if (pipe(cgi_input) < 0)
	{
		cannot_execute(client);
		return;
	}

	if ( (pid = fork()) < 0 )
	{
		cannot_execute(client);
		return;
	}

	// �ӽ�����ִ��cgi�ű���cgi�ű���������ݴ�ӡ����׼���
	if (pid == 0)   /* �ӽ�����: CGI script */
	{
		char meth_env[255];
		char query_env[255];
		char length_env[255];

		dup2(cgi_output[1], 1); // ��cgi_output��д����ض��򵽱�׼���
		dup2(cgi_input[0], 0);  // ��cgi_input��д����ض��򵽱�׼����
		close(cgi_output[0]); // ���ӽ����йر�cgi_output�Ķ�ȡ��
		close(cgi_input[1]);  // ���ӽ����йر�cgi_input��д���
		sprintf(meth_env, "REQUEST_METHOD=%s", method);
		putenv(meth_env);
		if (strcasecmp(method, "GET") == 0)
		{
			// �趨query_string��������
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		}
		else       /* POST method */
		{
			// �趨query_string��������

			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		execl(path, path, NULL); // ִ��cgi�ű�
		exit(0);
	}
	else {    /* in parent */   // �����̽�����������ͨ���ܵ����͸��ӽ���
		close(cgi_output[1]); // �ڸ������йر�cgi_output��д���
		close(cgi_input[0]);  // �ڸ������йر�cgi_output��д���
		if (strcasecmp(method, "POST") == 0)
		{
			for (i = 0; i < content_length; i++)
			{
				// �ӿͻ��˶�ȡһ�����ַ������ͨ���ܵ��ض���child�ı�׼����
				recv(client, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		}
		// ���ӹܵ��ж�ȡ�����ݷ��͵�client
		while (read(cgi_output[0], &c, 1) > 0)
			send(client, &c, 1, 0);
		// �ر������ܵ�������һ��
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
	}
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/* ��socket�ж�ȡһ�����ݣ����з�����Ϊ'\r', '\n'��'\r\n'. �����ȡ��������
* ��֮һ��ͳһ�趨Ϊ'\n'�������null�ַ���β. ���û�м�⵽���з���
* �ַ�����null��β
*/
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
		n = recv(sock, &c, 1, 0);
		/* DEBUG printf("%02X\n", c); */
		if (n > 0)
		{
			// ���ĩβ��\r��\r\n��ϣ���Ϊ\n
			if (c == '\r')
			{
				// MSG_PEEKѡ��ʹ����һ����Ȼ���Զ�ȡ����ַ�
				n = recv(sock, &c, 1, MSG_PEEK);
				/* DEBUG printf("%02X\n", c); */
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else     // ���û�пɶ��ģ���c����Ϊ\n����ֹѭ��
		{
			c = '\n';
		}
	}
	buf[i] = '\0';

	return i;
}

void options(int client)
{
	time_t now;
	char buf[1024];
	char buf1[1024] = {0};
	char timebuf[100] = {0};

	const char* rfc1123_fmt = "%a, %d %b %Y %H:%M:%S GMT";

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	sprintf(buf, "Allow: GET, HEAD, POST,OPTIONS\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	strcpy(buf, SERVER_STRING);
	sprintf(buf1, "%s%s", buf1, buf);

	sprintf(buf, "Public: GET, HEAD, POST,OPTIONS\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	now = time( (time_t*) 0 );
	(void) strftime( timebuf, sizeof(timebuf), rfc1123_fmt, gmtime( &now ));
	snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
	sprintf(buf1, "%s%s", buf1, buf);

	sprintf(buf, "Content-Length: 0\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	strcpy(buf, "\r\n");
	sprintf(buf1, "%s%s", buf1, buf);
	send(client, buf1, strlen(buf1), 0);

	memset(buf1, 0, sizeof(buf1));
	memset(buf, 0, sizeof(buf));
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
// �����ļ���http header
/**********************************************************************/
void headers(int client, const char *filename)
{
	char buf[1024] = {0};
	char buf1[1024] = {0};
	char timebuf[100] = {0};
	time_t now;
	(void)filename;  /* could use filename to determine file type */
	const char* rfc1123_fmt = "%a, %d %b %Y %H:%M:%S GMT";

	strcpy(buf, "HTTP/1.0 200 OK\r\n");/** ֻ��\r\n ��β*/
	send(client, buf, strlen(buf), 0);

	strcpy(buf, SERVER_STRING);
	sprintf(buf1, "%s%s", buf1, buf);

	now = time( (time_t*) 0 );
	(void) strftime( timebuf, sizeof(timebuf), rfc1123_fmt, gmtime( &now ));
	snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
	sprintf(buf1, "%s%s", buf1, buf);

	snprintf( buf, sizeof(buf), "Cache-Control: no-cache,no-store\r\n" );
	sprintf(buf1, "%s%s", buf1, buf);

	snprintf( buf, sizeof(buf), "Content-Encoding: %s\r\n", "gz" );
	sprintf(buf1, "%s%s", buf1, buf);

	struct stat sb;
	stat( filename, &sb );
	snprintf(buf, sizeof(buf), "Content-Length: %lld\r\n", (int64_t) sb.st_size );
	sprintf(buf1, "%s%s", buf1, buf);

	sprintf(buf, "Content-Type: text/html\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	snprintf( buf, sizeof(buf), "Connection: close\r\n" );
	sprintf(buf1, "%s%s", buf1, buf);

	strftime(timebuf, sizeof(timebuf), rfc1123_fmt, gmtime(&sb.st_mtime ) );
	snprintf( buf, sizeof(buf), "Last-Modified: %s\r\n\r\n", timebuf );

	sprintf(buf1, "%s%s", buf1, buf);
	send(client, buf1, strlen(buf1), 0);

	strcpy(buf, "\r\n");
	sprintf(buf1, "%s%s", buf1, buf);

	memset(buf1, 0, sizeof(buf1));
	memset(buf, 0, sizeof(buf));
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
// ��Ϥ�� 404 not found :)
/**********************************************************************/
void not_found(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
// ���ļ����͸�client
/**********************************************************************/
void serve_file(int client, const char *filename)
{
	FILE *resource = NULL;
	int numchars = 1;
	char buf[1024];

	buf[0] = 'A';
	buf[1] = '\0';
	while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
		numchars = get_line(client, buf, sizeof(buf));

	resource = fopen(filename, "r");
	if (resource == NULL)
		not_found(client);
	else
	{
		// �� header ���ļ����ݴ��͸� client
		headers(client, filename);
		cat(client, resource);
	}
	fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
// ����һ�������׽��֣��ȴ��ͻ�������
/**********************************************************************/
int startup(u_short *port)
{
	int httpd = 0;
	struct sockaddr_in name;

	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
		error_die("socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(11000);
	name.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY: ��ʾ�����ַ, see IP(7)
	if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
		error_die("bind");
	/* if dynamically allocating a port */
	if (*port == 0)
	{
		socklen_t namelen = sizeof(name);
		if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port);
	}
	if (listen(httpd, 5) < 0) // queue size if 5
		error_die("listen");

	return httpd;
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
// �ͻ�������û��ʵ��
/**********************************************************************/
void unimplemented(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
	int server_sock = -1;
	u_short port = 0;
	int client_sock = -1;
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	server_sock = startup(&port); // ����һ�������׽���
	printf("\033[34m\nhttpd running on port \033[33m%d\033[0m\n", port);
	fprintf(stdout, "\nPress Ctrl+C To Terminate.\n");
	while (1)
	{
		// ����һ���������׽���
		client_sock = accept(server_sock,
		                     (struct sockaddr *)&client_name,
		                     &client_name_len);
		if (client_sock == -1)
			error_die("accept");
		accept_request(client_sock); // ��������
	}

	close(server_sock);

	return 0;
}









