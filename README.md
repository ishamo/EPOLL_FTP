1. 该服务器端在处理方式上以守候进程方式运行，使用`epoll`编程模型来提供多个客户端的连接，在数据处理上，使用了简单的HTTP协议。

2. 编译与安装程序：

 - make
 - make install 

3. 配置：

配置文件为test_httpd.conf

home_dir=/var //供下载文件用的主目录，如果没有配置，则为/tmp

port=80		  //端口，保证当前系统没有使用此端口，默认为80

ip=127.0.0.1  //本机IP地址，如果不设置，程序将自己读取eth0地址

> 还有很多BUG, 我需要仔细调. 暂时先放在这里. 不要鄙视我啊... 
