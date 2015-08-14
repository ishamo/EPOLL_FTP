#include"test_httpd.h"

int main(int argc, char **argv)
{
	
	init_daemon(argv[0],LOG_INFO);

	if(get_arg("home_dir")==0) {
		sprintf(home_dir,"%s","/tmp");
	}

	if(get_arg("ip")==0) {
		get_addr("eth0");
	}

	if(get_arg("port")==0) {
		sprintf(port,"%s","80");
	}

	if(get_arg("back")==0) {
		sprintf(back,"%s","5");
	}

    int sock_fd = socket_bind(ip, atoi((const char *)&port));
    
	do_epoll(sock_fd);
    
    close(sock_fd);
	
    return 0;
} 

