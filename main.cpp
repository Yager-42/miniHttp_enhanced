#include <stdio.h>
#include <Winsock2.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#pragma comment(lib, "ws2_32.lib")
#define PRINTF(str) printf("[%s - %d]" #str "=%s\n", __func__, __LINE__, str);
void error_die(const char *str)
{
    perror(str);
    exit(1);
}

int startup(unsigned short *port)
{
    WSADATA data;
    int ret = WSAStartup(
        MAKEWORD(1, 1),
        &data);
    if (ret)
    {
        error_die("WSAStartup"); // ret!=0,报错
    }
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1)
    {
        error_die("套接字");
    }
    //设置端口复用
    int opt = 1;
    ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    if (ret == -1)
    {
        error_die("setsockopt");
    }
    //配置服务器端的网路地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(*port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //绑定套接字
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error_die("bind");
    }

    //动态获取端口号
    int nameLen = sizeof(server_socket);
    if (*port == 0)
    {
        if (getsockname(server_socket, (struct sockaddr *)&server_addr, &nameLen) < 0)
        {
            error_die("getsockname");
        }
        *port = server_addr.sin_port;
    }

    //创建监听队列
    if (listen(server_socket, 5) < 0)
    {
        error_die("listen");
    }
    return server_socket;
}

int get_line(int sock, char *buff, int size)
{
    char c = 0; //'\0'
    int i = 0;
    while (i < size - 1 && c != '\n')
    {
        int n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                //读取下一个字符，但是位置不变，相当于提前查看下一个字符，但是再调用recv还是读到当前读取的字符
                if (n > 0 && c == '\n')
                {
                    recv(sock, &c, 1, 0);
                }
                else
                {
                    c = '\n'; //读不到就不读了
                }
            }
            buff[i++] = c;
        }
        else
        {
            c = '\n'; //读不到就不读了
        }
    }
    buff[i] = 0;
    return i; //实际读到的数量
}

void unimplent(int client)
{
    char buf[1024];
 
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "Server: RockHttpd/0.1\r\n");
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

void not_found(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "Server: RockHttpd/0.1\r\n");
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

void headers(int client,const char* type)
{
    char buff[1024];
    //状态行
    strcpy(buff, "HTTP/1.0 200 OK\r\n");
    send(client, buff, strlen(buff), 0);

    strcpy(buff, "Server: RockHttpd/0.1\r\n");
    send(client, buff, strlen(buff), 0);

    char buf[1024];
    sprintf(buf, "Content-type: %s\n",type);
    send(client, buf, strlen(buf), 0);

    strcpy(buff, "\r\n");
    send(client, buff, strlen(buff), 0);
}

void cat(int client, FILE *resource)
{
    char buff[4096];
    int count = 0;
    while (1)
    {
        //从一个文件流中读数据,读取count个元素,每个元素size字节.
        //如果调用成功返回count.如果调用成功则实际读取size*count字节
        int ret = fread(buff, sizeof(char), sizeof(buff), resource);
        if (ret <= 0)
        {
            break;
        }
        send(client, buff, ret, 0);
        count += ret;
    }
    printf("count=%d\n", count);
}

const char* getHeaderType(const char* fileName)
{
    const char* ret = "text/html";
    const char* p = strrchr(fileName,'.');
    if(!p) return ret;
    p++;
    if(!strcmp(p,"css")) ret = "text/css";
    else if (!strcmp(p,"jpg")) ret = "image/jpeg";
    else if (!strcmp(p,"png")) ret = "image/png";
    else if (!strcmp(p,"js")) ret = "application/x-javascript";
    return ret;
}

void server_file(int client, const char *fileName)
{
    int numchars = 1;
    char buff[1024];
    //清空发送内容
    while (numchars > 0 && strcmp(buff, "\n"))
    {
        numchars = get_line(client, buff, sizeof(buff));
        PRINTF(buff);
    }
    //將文件加載進内存
    FILE *resource = fopen(fileName, "rb");
    if (resource == NULL)
    {
        not_found(client);
    }
    else
    {
        //先發送頭
        headers(client,getHeaderType(fileName));

        //发送资源
        cat(client, resource);

        printf("资源发送完毕\n");
    }
    fclose(resource);
}

//线程处理函数
DWORD WINAPI accept_request(LPVOID arg)
{
    char buff[1024];
    int client = (SOCKET)arg;

    int numchars = get_line(client, buff, sizeof(buff));
    PRINTF(buff);

    char method[255];
    int j = 0;
    int i = 0;
    while (!isspace(buff[j]) && i < sizeof(method) - 1)
    {
        method[i++] = buff[j++];
    }
    method[i] = 0;
    PRINTF(method);

    //检查请求方法
    if (stricmp(method, "GET") && strcmp(method, "POST"))
    {
        unimplent(client);
        return 0;
    }

    //解析资源文件路径
    //"GET /test/abc.html HTTP/1.1\n"
    char url[255];
    i = 0;
    while (isspace(buff[j]) && j < sizeof(method) - 1)
    {
        ++j;
    }
    while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(method) - 1)
    {
        url[i++] = buff[j++];
    }
    url[i] = 0;
    PRINTF(url)

    char path[512] = "";
    sprintf(path, "docs/demo%s", url);
    if (path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }
    PRINTF(path);

    //获取文件状态信息
    struct stat status;
    //找不到相应文件
    if (stat(path, &status) == -1)
    {
        //清空发送内容
        while (numchars > 0 && strcmp(buff, "\n"))
        {
            numchars = get_line(client, buff, sizeof(buff));
        }
        not_found(client);
    }
    else
    {
        //检查是文件还是目录
        if ((status.st_mode & S_IFMT) == S_IFDIR)
        {
            strcat(path, "/index.html");
        }
        server_file(client, path);
    }

    closesocket(client);
    return 0;
}

int main()
{
    unsigned short port = 80;
    int server_sock = startup(&port);
    printf("httpd服务已启动，正在监听%d 端口...\n", port);

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (1)
    {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == -1)
        {
            error_die("accept");
        }
        DWORD threadID = 0;
        CreateThread(0, 0, accept_request, (void *)client_sock, 0, &threadID);
    }
    closesocket(server_sock);
    return 0;
}