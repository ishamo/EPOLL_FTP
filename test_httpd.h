#include <pwd.h>
#include <grp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define MAXBUF        1024
#define MAXPATH        128

char buffer[MAXBUF + 1];
char ip[128];
char port[128];
char back[128];
char home_dir[128];


#define MAXSIZE     1024
#define LISTENQ     5
#define FDSIZE      1000
#define EPOLLEVENTS 100

void init_daemon(const char *pname, int facility);
int get_arg(char *cmd);
int get_addr(char *str);
char *dir_up(char *dirpath);
void wrtinfomsg(char *msg);
int socket_bind(const char* ip,int port);
void GiveResponse(FILE * client_sock, char *Path);
void wrtinfomsg(char *msg);
void handle_events(int epollfd,struct epoll_event *events,int num,int listenfd,char *buf);
void handle_accpet(int epollfd,int listenfd);
void do_read(int epollfd,int new_fd,char *buf);
void do_write(int epollfd,int fd,char *buf);

void add_event(int epollfd,int fd,int state);
void delete_event(int epollfd,int fd,int state);
void modify_event(int epollfd,int fd,int state);
void do_epoll(int listenfd);

void wrtinfomsg(char *msg)
{  
	syslog(LOG_INFO,"%s",msg);
}


int get_arg(char *cmd)
{
	FILE* fp;
	char buffer[1024];
	size_t bytes_read;
	char* match;
	fp = fopen("/etc/test_httpd.conf", "r");
	bytes_read = fread(buffer, 1, sizeof(buffer), fp);
	fclose(fp);

	if (bytes_read == 0 || bytes_read == sizeof(buffer))
		return 0;
	buffer[bytes_read] = '\0';

	if(!strncmp(cmd,"home_dir",8)){
		match = strstr (buffer, "home_dir=");
		if (match == NULL)
			return 0;
		bytes_read=sscanf(match,"home_dir=%s",home_dir);
		return bytes_read;
	}

	else if(!strncmp(cmd,"port",4)){
		match = strstr (buffer, "port=");
		if (match == NULL)
			return 0;
		bytes_read=sscanf(match,"port=%s",port);
		return bytes_read;
	}

	else if(!strncmp(cmd,"ip",2)){
		match = strstr (buffer, "ip=");
		if (match == NULL)
			return 0;
		bytes_read=sscanf(match,"ip=%s",ip);
		return bytes_read;
	}
	else if(!strncmp(cmd,"back",4)){
		match = strstr (buffer, "back=");
		if (match == NULL)
			return 0;
		bytes_read=sscanf(match,"back=%s",back);
		return bytes_read;
	}
	 else
		return 0;
}


char file_type(mode_t st_mode)
{
	if ((st_mode & S_IFMT) == S_IFSOCK)
		return 's';
	else if ((st_mode & S_IFMT) == S_IFLNK)
		return 'l';
	else if ((st_mode & S_IFMT) == S_IFREG)
		return '-';
	else if ((st_mode & S_IFMT) == S_IFBLK)
		return 'b';
	else if ((st_mode & S_IFMT) == S_IFCHR)
		return 'c';
	else if ((st_mode & S_IFMT) == S_IFIFO)
		return 'p';
	else
		return 'd';
}

//获得当前路径的目录
char *dir_up(char *dirpath)
{
    static char Path[MAXPATH];
    int len;

    strcpy(Path, dirpath);
    len = strlen(Path);
    if (len > 1 && Path[len - 1] == '/')
        len--;
    while (Path[len - 1] != '/' && len > 1)
        len--;
    Path[len] = 0;
    return Path;
}


//发送path里面的数据到客户端,如果path是一个文件,那就发送数据,如果是目录,那就列出目录
void GiveResponse(FILE * client_sock, char *Path)
{
    struct dirent *dirent;
    struct stat info;
    char Filename[MAXPATH];
    DIR *dir;
    int fd, len, ret;
    char *p, *realPath, *realFilename, *nport;

	struct passwd *p_passwd;
	struct group *p_group;
	char *p_time;	

	//得到目录或文件
    len = strlen(home_dir) + strlen(Path) + 1;
    realPath = malloc(len + 1);
    bzero(realPath, len + 1);
    sprintf(realPath, "%s/%s", home_dir, Path);

	//得到端口
    len = strlen(port) + 1;
    nport = malloc(len + 1);
    bzero(nport, len + 1);
    sprintf(nport, ":%s", port);


	//得到dir或file的文件状态
    if (stat(realPath, &info)) {
        fprintf(client_sock,
                "HTTP/1.1 200 OK\r\nServer:Test http server\r\nConnection: close\r\n\r\n<html><head><title>%d - %s</title></head>"
                "<body><font size=+4>Linux HTTP server</font><br><hr width=\"100%%\"><br><center>"
                "<table border cols=3 width=\"100%%\">", errno,
                strerror(errno));
        fprintf(client_sock,
                "</table><font color=\"CC0000\" size=+2> connect to administrator, error code is: \n%s %s</font></body></html>",
                Path, strerror(errno));
        goto out;
    }

	//如果是文件的话
    if (S_ISREG(info.st_mode)) 
	{
        fd = open(realPath, O_RDONLY);
        len = lseek(fd, 0, SEEK_END);
        p = (char *) malloc(len + 1);
        bzero(p, len + 1);
        lseek(fd, 0, SEEK_SET);
        ret = read(fd, p, len);
        close(fd);
        fprintf(client_sock,
                "HTTP/1.1 200 OK\r\nServer: Test http server\r\nConnection: keep-alive\r\nContent-type: application/*\r\nContent-Length:%d\r\n\r\n", len);
       
		fwrite(p, len, 1, client_sock);
        free(p);
    } 
	else if (S_ISDIR(info.st_mode)) 
	{

	//如果是目录,那就列出来
        dir = opendir(realPath);
        fprintf(client_sock,
                "HTTP/1.1 200 OK\r\nServer:Test http server\r\nConnection:close\r\n\r\n<html><head><title>%s</title></head>"
                "<body><font size=+4>Linux HTTP server file</font><br><hr width=\"100%%\"><br><center>"
                "<table border cols=3 width=\"100%%\">", Path);
        fprintf(client_sock,
                "<caption><font size=+3> Directory %s</font></caption>\n",
                Path);
        fprintf(client_sock,
                "<tr><td>name</td><td>type</td><td>owner</td><td>group</td><td>size</td><td>modify time</td></tr>\n");
        if (dir == NULL) {
            fprintf(client_sock, "</table><font color=\"CC0000\" size=+2>%s</font></body></html>",
                    strerror(errno));
            return;
        }
        while ((dirent = readdir(dir)) != NULL) 
		{
            if (strcmp(Path, "/") == 0)
                sprintf(Filename, "/%s", dirent->d_name);
            else
                sprintf(Filename, "%s/%s", Path, dirent->d_name);
            if(dirent->d_name[0]=='.')
				continue;
		    fprintf(client_sock, "<tr>");
            len = strlen(home_dir) + strlen(Filename) + 1;
            realFilename = malloc(len + 1);
            bzero(realFilename, len + 1);
            sprintf(realFilename, "%s/%s", home_dir, Filename);
            if (stat(realFilename, &info) == 0) 
			{
                if (strcmp(dirent->d_name, "..") == 0)
                    fprintf(client_sock, "<td><a href=\"http://%s%s%s\">(parent)</a></td>",
                            ip, atoi(port) == 80 ? "" : nport,dir_up(Path));
                else
                    fprintf(client_sock, "<td><a href=\"http://%s%s%s\">%s</a></td>",
                            ip, atoi(port) == 80 ? "" : nport, Filename, dirent->d_name);
               


	          	p_time = ctime(&info.st_mtime);
	           	p_passwd = getpwuid(info.st_uid);	
	           	p_group = getgrgid(info.st_gid);

			  	fprintf(client_sock, "<td>%c</td>", file_type(info.st_mode));
			   	fprintf(client_sock, "<td>%s</td>", p_passwd->pw_name);
		      	fprintf(client_sock, "<td>%s</td>", p_group->gr_name);
			  	fprintf(client_sock, "<td>%d</td>", (int)(info.st_size));
				fprintf(client_sock, "<td>%s</td>", ctime(&info.st_ctime));
            }
            fprintf(client_sock, "</tr>\n");
            free(realFilename);
        }
        fprintf(client_sock, "</table></center></body></html>");
    } else {
      //禁止访问其它内容
        fprintf(client_sock,
                "HTTP/1.1 200 OK\r\nServer:Test http server\r\nConnection: close\r\n\r\n<html><head><title>permission denied</title></head>"
                "<body><font size=+4>Linux HTTP server</font><br><hr width=\"100%%\"><br><center>"
                "<table border cols=3 width=\"100%%\">");
        fprintf(client_sock,
                "</table><font color=\"CC0000\" size=+2> you access resource '%s' forbid to access,communicate with the admintor </font></body></html>",
                Path);
    }
  out:
    free(realPath);
    free(nport);
}


void init_daemon(const char *pname, int facility)
{
	int pid; 
	int i;
	signal(SIGTTOU,SIG_IGN); 
	signal(SIGTTIN,SIG_IGN); 
	signal(SIGTSTP,SIG_IGN); 
	signal(SIGHUP ,SIG_IGN);
	if(pid=fork()) 
		exit(EXIT_SUCCESS); 
	else if(pid< 0){
		perror("fork");
		exit(EXIT_FAILURE);
	}
	setsid(); 
	if(pid=fork()) 
		exit(EXIT_SUCCESS); 
	else if(pid< 0){
		perror("fork");
		exit(EXIT_FAILURE);
	}  
	for(i=0;i< NOFILE;++i)
		close(i);
	chdir("/tmp"); 
	umask(0);  
	signal(SIGCHLD,SIG_IGN);
	openlog(pname, LOG_PID, facility);
	return; 
} 


int get_addr(char *str)
{
	int inet_sock;
	struct ifreq ifr;
	inet_sock = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, str);
	if (ioctl(inet_sock, SIOCGIFADDR, &ifr) < 0){
		wrtinfomsg("bind");
		exit(EXIT_FAILURE);
	}
	sprintf(ip,"%s", inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
}

//创建套接字并进行绑定监听
int socket_bind(const char* ip,int port)
{
    int  listenfd, addrlen;
    struct sockaddr_in servaddr;
	int ret;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if (listenfd == -1){
		wrtinfomsg("socket()");
        exit(EXIT_FAILURE);
    }

	addrlen = 1;	
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &addrlen, sizeof(addrlen));

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&servaddr.sin_addr);
    servaddr.sin_port = htons(atoi((const char*)&port));
	addrlen = sizeof(struct sockaddr_in);
    if ((ret = bind(listenfd, (struct sockaddr*)&servaddr, addrlen))== -1){
		wrtinfomsg("bind");
        exit(EXIT_FAILURE);
    }
	
	if (listen(listenfd, atoi(back)) < 0){
		wrtinfomsg("listen");
		exit(EXIT_FAILURE);
	}
    return listenfd;
}

//IO多路复用epoll
void do_epoll(int listenfd)
{
    int epollfd;
    struct epoll_event events[EPOLLEVENTS];
    int ret;
    char buf[MAXSIZE];
    memset(buf,0,MAXSIZE);
    //创建一个描述符
    epollfd = epoll_create(FDSIZE);
    //添加监听描述符事件
    add_event(epollfd,listenfd,EPOLLIN);
    for ( ; ; ){
        //获取已经准备好的描述符事件
        ret = epoll_wait(epollfd,events,EPOLLEVENTS,-1);
        handle_events(epollfd,events,ret,listenfd,buf);
    }
    close(epollfd);
}

//事件处理函数
void
handle_events(int epollfd,struct epoll_event *events,int num,int listenfd,char *buf)
{
    int i;
    int fd;
    //进行选好遍历
    for (i = 0;i < num;i++){
        fd = events[i].data.fd;
        //根据描述符的类型和事件类型进行处理
        if ((fd == listenfd) &&(events[i].events & EPOLLIN))
            handle_accpet(epollfd,listenfd);
        else if (events[i].events & EPOLLIN)
            do_read(epollfd,fd,buf);
        else if (events[i].events & EPOLLOUT)
            do_write(epollfd,fd,buf);
    }
}

//处理接收到的连接
void handle_accpet(int epollfd,int listenfd)
{
    int clifd;
    struct sockaddr_in cliaddr;
    socklen_t  cliaddrlen;
    clifd = accept(listenfd,(struct sockaddr*)&cliaddr,&cliaddrlen);
    if (clifd == -1)
        wrtinfomsg("accept");
    else{
		bzero(buffer, MAXBUF + 1);
        sprintf(buffer, "connect come from: %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        wrtinfomsg(buffer);

        //添加一个客户描述符和事件
        add_event(epollfd,clifd,EPOLLIN);
    }
}

void do_read(int epollfd,int new_fd,char *buf)
{
    int len;
	bzero(buffer, MAXBUF+1);
	if ((len = recv(new_fd, buffer, MAXBUF, 0)) > 0){
		
        modify_event(epollfd,new_fd,EPOLLOUT);

	}else if (len == -1){
		wrtinfomsg("read error");
        close(new_fd);
        delete_event(epollfd,new_fd,EPOLLIN);
    } else if (len == 0){
		wrtinfomsg("client close");
        close(new_fd);
        delete_event(epollfd,new_fd,EPOLLIN);
    }
}


void do_write(int epollfd,int fd,char *buf)
{
	FILE *ClientFD = fdopen(fd, "w");
	if (ClientFD == NULL){
		wrtinfomsg("fdopen");
		delete_event(epollfd,fd,EPOLLOUT);
	}else {
		char Req[MAXPATH + 1] = "";
		sscanf(buffer, "GET %s HTTP", Req);
		bzero(buffer, MAXBUF + 1);
		sprintf(buffer, "Reuquest get the file: \"%s\"\n", Req);
		wrtinfomsg(buffer);
		GiveResponse(ClientFD, Req);
		fclose(ClientFD);
	}
	modify_event(epollfd,fd,EPOLLIN);
}

//添加事件
void add_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}

//删除事件
void delete_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}

//修改事件
void modify_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev);
}
