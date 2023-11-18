//
// Created by KyrieGYJ on 2023/11/18.
//
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>

#define SERVER_STRING "Server: xhttpd/0.1.0\r\n"

/**
 * 启动服务器：创建socket -> bind端口 -> listen
 * @param port
 * @return
 */
int startup(u_short *port) {
    struct sockaddr_in name;
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("failed to create socket(file descriptor).");
        exit(1);
    }
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port); // host to network short. 计算机字节序与网络字节序不同
    printf("name.sin_port: %hu.\r\n", name.sin_port);
    name.sin_addr.s_addr = htonl(INADDR_ANY); // 本机任意地址
    int nameLen = sizeof(name);
    if (bind(fd, (struct sockaddr *)&name, nameLen) < 0) {
        perror("failed to bind fd with socketaddr_in");
        exit(1);
    }
    // sockaddr.sin_port为0时，系统自动分配空闲端口。
    if (*port == 0 && getsockname(fd, (struct sockaddr *)&name, (socklen_t *)&nameLen) == -1) {
        perror("failed to getsockname.");
        exit(1);
    }
    *port = ntohs(name.sin_port);
    printf("port: %hu.\r\n", *port);
    if (listen(fd, 5) < 0) {
        perror("failed to listen fd.");
        exit(1);
    }
    return fd;
}

/**
 * 从socket中读取一行数据。
 * 终止条件：1、换行符 2、回车符 3、CRLF（换行+回车）4、缓冲区结束
 * 字符串统一以换行符+空字符终止'\0'，取代原有终止符。
 * @param sock
 * @param *buf
 * @param size
 */
int read_line(int sock, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int read_len;
    while (i < size - 1 && c != '\n') {
        read_len = recv(sock, &c, 1, 0);
        if (read_len < 0) {
            c = '\n';
            continue;
        }
        if (c == '\r') {
            read_len = recv(sock, &c, 1, MSG_PEEK); // MSG_PEEK指不从队列中删除消息，只查看。
            if (read_len > 0 && c == '\n') {
                recv(sock, &c, 1, 0);
            } else {
                c = '\n';
            }
        }
        buf[i] = c;
        i++;
    }
    buf[i] = '\0';
    return i;
}

/**
 *
 * @param client
 */
void respond_501(int client_sock) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client_sock, buf, strlen(buf), 0);
}

void respond_404(int client_sock) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client_sock, buf, strlen(buf), 0);
}

void respond_200_header(int client_sock, const char *filename) {
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client_sock, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sock, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client_sock, buf, strlen(buf), 0);
}

void print_file_stat(struct stat fileStat) {
    printf("文件大小: %lld bytes\n", fileStat.st_size);
    printf("文件权限: %o\n", fileStat.st_mode);
    printf("文件inode编号: %llu\n", fileStat.st_ino);
    printf("硬链接数量: %hu\n", fileStat.st_nlink);
    printf("文件创建时间: %ld\n", fileStat.st_ctime);
    printf("文件最后修改时间: %ld\n", fileStat.st_mtime);
    printf("文件最后访问时间: %ld\n", fileStat.st_atime);
    printf("inode最后修改时间: %ld\n", fileStat.st_ctime);
    printf("块大小: %d bytes\n", fileStat.st_blksize);
    printf("块数量: %lld\n", fileStat.st_blocks);
}

/**
 * 将整个文件的内容返回客户端。
 * @param client_sock 客户端套接字
 * @param resource 文件指针
 */
void cat(int client_sock, FILE *resource) {
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource)) {
        send(client_sock, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void serve_file(int client_sock, const char *filename) {
    FILE *resource = NULL;
    int str_len = 1;
    char buf[1024];

    while (str_len > 0 && strcmp("\n", buf)) { // read & discard headers
        str_len = read_line(client_sock, buf, sizeof(buf));
        printf("read & discard %s.\n", buf);
    }

    resource = fopen(filename, "r");
    if (resource == NULL) {
        printf("respond 404.");
        respond_404(client_sock);
    } else {
        printf("respond 200.");
        respond_200_header(client_sock, filename);
        cat(client_sock, resource);
    }
    fclose(resource);
}

/**
 * 解析请求头
 * @param client_sock
 */
void parse_request(int client_sock) {
    printf("start parsing request...\n");
    char buf[1024];
    int str_len = read_line(client_sock, buf, sizeof(buf));
    size_t i = 0, j = 0;

    // parse method
    char method[255];
    while (!isspace(buf[j]) && (i < sizeof(method) - 1)) {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        printf("Unimplemented method: %s.", method);
        respond_501(client_sock);
        return;
    }

    // skip extra space
    i = 0;
    while (!isspace(buf[j]) && j < sizeof(buf)) {
        j++;
    }

    // parse url
    char url[255];
    while (!isspace(buf[j]) && i < sizeof(url) - 1 && j < sizeof(buf)) {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    // extract query_string
    char *query_string = NULL;
    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while (*query_string != '?' && *query_string != '\0') {
            query_string++;
        }
        if (*query_string == '?') {
            *query_string = '\0';
            query_string++;
        }
    }

    // parse path
    char path[512];
    sprintf(path, "statics%s", url);
    if (path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
    }
    printf("client visit path: %s.\n", path);

    // process request
    struct stat st;
    if (stat(path, &st) == -1) {
        // read & discard headers
        while (str_len > 0 && strcmp("\n", buf)) {
            str_len = read_line(client_sock, buf, sizeof(buf));
        }
        respond_404(client_sock);
    } else {
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            strcat(path, "/index.html");
        }
        //  check privilege
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {
            // execute script file (NOT SUPPORTED YET!)
        }
        printf("final path: %s.\n", path);
        serve_file(client_sock, path);
    }
    print_file_stat(st);
    close(client_sock);
}

int main() {
    u_short port = 0; // 端口号
    int client_sock = -1;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);
    int server_sock = startup(&port);
    printf("httpserver running on port %d.\n", port);
    while(1) {
        printf("waiting for connect.\n");
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
        printf("received connection from ...\n");
        if (client_sock == 1) {
            perror("failed to accept client request.");
            exit(1);
        }
        parse_request(client_sock);
    }

}