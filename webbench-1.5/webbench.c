/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess SUCESS
 *    1 - benchmark failed (server is not on-line) BENCHMARK_FAILED
 *    2 - bad param BAD_PARAM
 *    3 - internal error, fork failed INTERNAL_ERROR
 * 
 */ 

#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired = 0;
int speed = 0;
int failed = 0;
int bytes = 0;

/* globals */
int http10 = 1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */

/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"

// 和选项有关的变量，请看main函数while语句
int method = METHOD_GET;
int clients = 1; // 同时运行client的个数
int force = 0;
int force_reload = 0;
int proxyport = 80;
char *proxyhost = NULL;
int benchtime = 30;

/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];

static const struct option long_options[]=
{
    {"force", no_argument, &force, 1},
    {"reload", no_argument, &force_reload, 1},
    {"time", required_argument, NULL, 't'},
    {"help", no_argument, NULL, '?'},
    {"http09", no_argument, NULL, '9'},
    {"http10", no_argument, NULL, '1'},
    {"http11", no_argument, NULL, '2'},
    {"get", no_argument, &method, METHOD_GET},
    {"head", no_argument, &method, METHOD_HEAD},
    {"options", no_argument, &method, METHOD_OPTIONS},
    {"trace", no_argument, &method, METHOD_TRACE},
    {"version", no_argument, NULL, 'V'},
    {"proxy", required_argument, NULL, 'p'},
    {"clients", required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
    if (signal == SIGALRM)
        timerexpired = 1;
}	

static void usage(void)
{
    fprintf(stderr,
            "webbench [option]... URL\n"
            "  -f|--force               Don't wait for reply from server.\n"
            "  -r|--reload              Send reload request - Pragma: no-cache.\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -p|--proxy <server:port> Use proxy server for request.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -9|--http09              Use HTTP/0.9 style requests.\n"
            "  -1|--http10              Use HTTP/1.0 protocol.\n"
            "  -2|--http11              Use HTTP/1.1 protocol.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  --options                Use OPTIONS request method.\n"
            "  --trace                  Use TRACE request method.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n"
           );
};


int main(int argc, char *argv[])
{
    int opt = 0;
    int options_index = 0;
    char *tmp = NULL;

    if (argc == 1) {
        usage();
        return 2;
    } 
    
    /* 解析选项并执行相应的操作 */
    while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &options_index)) != EOF)
    {
        switch (opt) {
            case  0 : break;
            case 'f': force = 1; break;
            case 'r': force_reload = 1; break; 
            case '9': http10 = 0; break;
            case '1': http10 = 1; break;
            case '2': http10 = 2; break;
            case 'V': printf(PROGRAM_VERSION"\n"); exit(0);
            case 't': benchtime = atoi(optarg); break;	     
            case 'p': 
                      /* proxy server parsing server:port */
                      tmp = strrchr(optarg, ':'); // port number follows ':'
                      proxyhost = optarg;
                      if (tmp == NULL) {
                          break;
                      }
                      if (tmp == optarg) {
                          fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
                          return 2;
                      }
                      if (tmp == optarg + strlen(optarg) - 1) {
                          fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
                          return 2;
                      }
                      // 获取proxyhost和proxyport
                      *tmp = '\0';
                      proxyport = atoi(tmp + 1);
                      break;
            case ':':
            case 'h':
            case '?': usage(); return 2; break;
            case 'c': clients = atoi(optarg); break;
        }
    }

    // 没有url，返回错误
    if (optind == argc) {
        fprintf(stderr, "webbench: Missing URL!\n");
        usage();
        return 2;
    }

    // 设定默认参数
    if (clients == 0)
        clients = 1;
    if (benchtime == 0)
        benchtime = 60;
    /* Copyright */
    fprintf(stderr, "Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
            "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
           );
    // 根据参数，构建请求字符串
    build_request(argv[optind]);
    /* print bench info */
    // 打印bench的信息，这些信息都出现在运行bench之前
    printf("\nBenchmarking: ");
    // method为全局变量，build_request中会修改它
    switch (method) {
        case METHOD_GET:
        default:
            printf("GET"); break;
        case METHOD_OPTIONS:
            printf("OPTIONS"); break;
        case METHOD_HEAD:
            printf("HEAD"); break;
        case METHOD_TRACE:
            printf("TRACE"); break;
    }
    printf(" %s", argv[optind]);
    // http版本
    switch (http10) {
        case 0: printf(" (using HTTP/0.9)"); break;
        case 2: printf(" (using HTTP/1.1)"); break;
    }
    printf("\n");
    if (clients == 1)
        printf("1 client");
    else
        printf("%d clients", clients);

    printf(", running %d sec", benchtime);
    if (force)
        printf(", early socket close");
    if (proxyhost != NULL)
        printf(", via proxy server %s:%d", proxyhost, proxyport);
    if (force_reload)
        printf(", forcing reload");
    printf(".\n");
    // 运行最核心的bench函数
    return bench();
}

/******************************************************************
 * build_request函数根据配置参数以及url构造一个request字符串
 * 修改的是全局数组request
 *****************************************************************/
void build_request(const char *url)
{
    char tmp[10];
    int i;

    // 清零
    bzero(host, MAXHOSTNAMELEN);
    bzero(request, REQUEST_SIZE);

    // 设置http版本号
    if (force_reload && proxyhost != NULL && http10 < 1)
        http10 = 1;
    if (method == METHOD_HEAD && http10 < 1)
        http10 = 1;
    if (method == METHOD_OPTIONS && http10 < 2)
        http10 = 2;
    if (method == METHOD_TRACE && http10 < 2)
        http10 = 2;
    // request第一个单词为请求方法
    switch (method) {
        default:
        case METHOD_GET: strcpy(request,"GET"); break;
        case METHOD_HEAD: strcpy(request,"HEAD"); break;
        case METHOD_OPTIONS: strcpy(request,"OPTIONS"); break;
        case METHOD_TRACE: strcpy(request,"TRACE"); break;
    }

    strcat(request, " ");

    // url中必须包含://
    if (NULL == strstr(url, "://")) {
        fprintf(stderr, "\n%s: is not a valid URL.\n",url);
        exit(2);
    }
    // url不能太长
    if (strlen(url) > 1500) {
        fprintf(stderr,"URL is too long.\n");
        exit(2);
    }
    // 如果没有设定proxy，且url不是http地址，错误
    if (proxyhost == NULL) {
        if (0 != strncasecmp("http://", url, 7)) {
            fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
            exit(2);
        }
    }

    // 如果url为 http://www.google.com/，则i值为7，即第一个w的位置
    /* protocol/host delimiter */
    i = strstr(url, "://") - url + 3;
    /* printf("%d\n",i); */

    // url必须以'/'结尾
    if (strchr(url + i, '/') == NULL) {
        fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }

    // 从url构建host 
    if (proxyhost == NULL) {
        /* get port from hostname */
        if (index(url + i, ':') != NULL &&
                index(url + i, ':') < index(url + i, '/'))
        {
            strncpy(host, url + i, strchr(url + i, ':') - url - i);
            bzero(tmp, 10);
            strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);
            /* printf("tmp=%s\n",tmp); */
            proxyport = atoi(tmp);
            // Web服务器默认端口为80
            if (proxyport == 0)
                proxyport = 80;
        } else {
            strncpy(host, url + i, strcspn(url + i, "/"));
        }
        // printf("Host=%s\n",host);
        strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
    } else {
        // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
        strcat(request, url);
    }
    // HTTP版本
    if (http10 == 1)
        strcat(request, " HTTP/1.0");
    else if (http10 == 2)
        strcat(request, " HTTP/1.1");
    strcat(request,"\r\n");
    // User-Agent
    if (http10 > 0)
        strcat(request, "User-Agent: WebBench "PROGRAM_VERSION"\r\n");
    // Host
    if (proxyhost == NULL && http10 > 0) {
        strcat(request, "Host: ");
        strcat(request, host);
        strcat(request, "\r\n");
    }
    // 
    if (force_reload && proxyhost != NULL)
        strcat(request, "Pragma: no-cache\r\n");
    // HTTP/1.1选项 响应完成后关闭连接
    if (http10 > 1)
        strcat(request, "Connection: close\r\n");
    /* add empty line at end */
    if (http10 > 0)
        strcat(request, "\r\n"); 
    // printf("Req=%s\n",request);
}

/* vraci system rc error kod */
static int bench(void)
{
    int i, j, k;	
    pid_t pid = 0;
    FILE *f;

    /* check avaibility of target server */
    /* 检查目标服务器是否可用 */
    i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
    if (i < 0) { 
        fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);

    /* create pipe */
    if (pipe(mypipe)) {
        perror("pipe failed.");
        return 3;
    }

    /* not needed, since we have alarm() in childrens */
    /* wait 4 next system clock tick */
    /*
       cas = time(NULL);
       while(time(NULL) == cas)
       sched_yield();
       */

    /* fork childs */
    // 创建多个子进程
    for (i = 0; i < clients; i++) {
        pid = fork();
        if (pid <= (pid_t) 0) {
            /* child process or error*/
            sleep(1); /* make childs faster */
            break; // 如果是child就break，否则会继续fork了
        }
    }

    if (pid < (pid_t) 0) {
        fprintf(stderr, "problems forking worker no. %d\n",i);
        perror("fork failed.");
        return 3;
    }
    
    if (pid == (pid_t) 0) {  // 子进程，运行benchcore，将结果通过管道发送给父进程
        // 运行benchcore
        if (proxyhost == NULL)
            benchcore(host, proxyport, request);
        else
            benchcore(proxyhost, proxyport, request);

        /* write results to pipe */
        // 通过管道将结果发给父进程
        f = fdopen(mypipe[1], "w");
        if (f == NULL) {
            perror("open pipe for writing failed.");
            return 3;
        }
        /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
        fprintf(f, "%d %d %d\n", speed, failed, bytes);
        fclose(f);
        return 0;
    } else {  // 父进程，从管道接收结果，并统计
        f = fdopen(mypipe[0], "r");
        if (f == NULL) {
            perror("open pipe for reading failed.");
            return 3;
        }
        setvbuf(f, NULL, _IONBF, 0); // 设置缓冲区，unbuffered，立刻读到数据
        speed = 0;
        failed = 0;
        bytes = 0;

        while (1) {
            // 读取子进程发过来的数据
            pid = fscanf(f, "%d %d %d", &i, &j, &k);
            if (pid < 2) {
                fprintf(stderr, "Some of our childrens died.\n");
                break;
            }
            speed += i;
            failed += j;
            bytes += k;
            /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
            // 每收到一个child的数据，clients计数减一
            if (--clients == 0)
                break;
        }
        fclose(f);

        // 打印结果
        printf("\nSpeed = %d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
                (int)((speed + failed) / (benchtime / 60.0f)), // 每分钟的请求数
                (int)(bytes / (float) benchtime), // 每秒服务器返回的字节数
                speed, // 成功请求数
                failed); // 失败请求数
    }
    return i;
}

void benchcore(const char *host, const int port, const char *req)
{
    int rlen;
    char buf[1500];
    int s, i;
    struct sigaction sa;

    /* setup alarm signal handler */
    // 处理超时
    sa.sa_handler = alarm_handler; // 信号处理函数
    sa.sa_flags = 0; // 
    if (sigaction(SIGALRM, &sa, NULL))
        exit(3); 
    // benchtime秒之后发送一个SIGALRM信号，信号处理函数将timerexpired置为1
    alarm(benchtime);

    rlen = strlen(req);
nexttry:
    while (1) {
        // 到时间了
        if (timerexpired) {
            if (failed > 0) {
                /* fprintf(stderr,"Correcting failed by signal\n"); */
                failed--;
            }
            return;
        }
        // 建立连接
        s = Socket(host, port);                          
        if (s < 0) {
            failed++;
            continue;
        } 
        // 发送请求给服务器
        if (rlen != write(s, req, rlen)) {
            failed++;
            close(s);
            continue;
        }
        // 如果是http 0.9，半关闭套接字
        if (http10 == 0) {
            // 半关闭失败，执行close
            if (shutdown(s, SHUT_WR)) {
                failed++;
                close(s);
                continue;
            }
        }
        // 默认情况下force为0，从服务器读取回复，否则不读取
        // 请看-f选项的解释
        if (force == 0) {
            /* read all available data from socket */
            while (1) {
                if (timerexpired)
                    break; 
                // 从服务器发送过来的数据
                i = read(s, buf, 1500);
                /* fprintf(stderr,"%d\n",i); */
                if (i < 0) {
                    // 读取失败
                    failed++;
                    close(s);
                    goto nexttry;
                } else {
                    if (i == 0) // 读完了
                        break;
                    else
                        bytes += i;
                }
            }
        }
        if (close(s)) {
            failed++;
            continue;
        }
        speed++;
    }
}
