/*
 * =====================================================================================
 *
 *       Filename:  jksutils.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/14/16 21:23:41
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Normandie (sams), romalin99@gmail.com
 *   Organization:  |ORGANIZATION DESCRITIONS|
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "ethicallutils.h"

std::string loadfile(const char *name, bool binary)
{
	std::ifstream ifs(name, binary ? std::ifstream::binary : std::ifstream::in);
	if (!ifs.is_open()) return std::string("");
	std::stringstream strSS;
	strSS << ifs.rdbuf();
	if( ifs.bad() ) return std::string("");
	std::string tmp;
	tmp = strSS.str();
	ifs.close();
	return  tmp;
}

std::string get_selfpath()
{
#define MAX_SIZE 256
	char buff[MAX_SIZE]{0};
	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
	if(len < 0 || len >= MAX_SIZE)
	{
		LOG(ERROR)<<"readlink /proc/self/exe failed!";
		exit(-1);
	}
	else
	{
		size_t  idx = len;
		for( ;buff[idx] != '/'; idx--) buff[idx]='\0';
		return std::string(buff);
	}
}

void init_daemon()
{	
	//step1 Catch signals 
	signal(SIGTSTP, SIG_IGN); //
	signal(SIGUSR1, SIG_IGN); //
	signal(SIGUSR2, SIG_IGN); //
	signal(SIGINT,  SIG_IGN); // 终端中断
	signal(SIGHUP,  SIG_IGN); // 连接挂断
	signal(SIGQUIT, SIG_IGN); // 终端退出
	signal(SIGPIPE, SIG_IGN); // 向无读进程的管道写数据
	signal(SIGTTOU, SIG_IGN); // 后台程序尝试写操作
	signal(SIGTTIN, SIG_IGN); // 后台程序尝试读操作
	signal(SIGTERM, SIG_IGN); // 终止
	
	char emsg[1024]{0};
	//step2  调用setsid() 的不能是进程组组长，当前程序有可能是进程组组长
	pid_t pid = fork();
	if(pid != 0) //非子进程则退出
	{
		strerror_r(errno, emsg, sizeof(emsg));
		//LOG(ERROR)<<emsg;
		exit(EXIT_FAILURE);
	}
	//父进程退出，留下子进程

	//step3  创建一个新的会话期，从而脱离原有的会话期, 进程同时与控制终端脱离
	setsid();

	//step4  此时子进程成为新会话期的领导和新的进程组长, 但这样的身份可能会通过fcntl去获到终端
	pid = fork();
	if(pid != 0) //非子进程则退出
	{
		memset(emsg, 0x00, sizeof(emsg));
		strerror_r(errno, emsg, sizeof(emsg));
		//LOG(ERROR)<<emsg;
		exit(EXIT_FAILURE);
	}
	//此时留下来的是孙子进程,再也不能获取终端

	// step5 通常来讲, 守护进程应该工作在一个系统永远不会删除的目录下
	{
		char szPath[1024]{0};
		if(getcwd(szPath, sizeof(szPath)) == nullptr)
		{
			memset(emsg, 0x00, sizeof(emsg));
			strerror_r(errno, emsg, sizeof(emsg));
			//LOG(ERROR)<<emsg;
			exit(EXIT_FAILURE);
		}
		else
		{
			chdir(szPath);
			//LOG(INFO)<<"set current path: "<<szPath;
		}
		//if(ischdir == 0) { chdir("/"); }
	}

	//step6 关闭输入输出和错误流 (通过日志来查看状态)
	int fd;
	if((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		//close(0);close(1); close(2);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) close(fd);
	}
	/* 关闭打开的文件描述符,包括标准输入、标准输出和标准错误输出 */
	//for(int fd = 0, fdtablesize = getdtablesize(); fd < fdtablesize; fd++)
	//close(fd);

	//step7  去掩码位
	umask((mode_t)0);
	signal(SIGCHLD,SIG_IGN); // 忽略SIGCHLD信号 
	syslog(LOG_USER|LOG_INFO,"cacheproxy daemo beginning ...\n"); //打开log系统
	
	//step8 set core dump file maximize
	struct rlimit rlim, rlim_new;
	if(getrlimit(RLIMIT_CORE, &rlim))
	{
		rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
		if(setrlimit(RLIMIT_CORE, &rlim_new))
		{
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			(void)setrlimit(RLIMIT_CORE, &rlim_new);
		}
	}
}


std::string  get_uuid()
{
	char buf[128]{0};
    uuid_t id;
    if(!uuid_generate_time_safe(id))
    {
        std::random_device rd;
        std::default_random_engine ger(rd());
        std::uniform_real_distribution<> ue(1, 4294967296);
        uint32_t te = ue(ger);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts); //CLOCK_MONOTONIC
        std::string sysnanos = std::to_string(ts.tv_sec * 1000000000 + ts.tv_nsec);
        uint32_t len = 36 - sysnanos.length();
        snprintf(buf,sizeof(buf),"%.*u",len, te);
        return std::string(sysnanos.append(buf));
    }
    else{
        uuid_unparse(id, buf);
        return std::string(buf);
    }
}
