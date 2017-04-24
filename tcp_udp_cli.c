#include "define_all_header.h"
//定义数据报类型
typedef enum type_pack
{
	HEARD,//心跳报
	DATA //数据报
}type_pack;
//客户信息
typedef struct client_info
{
	char cli_id[10];
}client_info;
//定义发送数据报结构
typedef struct send_pack
{
	type_pack type;
	client_info meg_send;//发送方id
	client_info meg_recv;//接收方id
	int is_ack;//发送验证位
	char msg_data[1024];//发送的数据
	char filename[20];//文件名称,
	int fd;
} send_pack;
//用链表管理好友fd
//定义链表节点
typedef struct list_node
{
	char cli_friend_id[10];
	struct list_node* next;
}list_node;
//定义链表
typedef struct link_list
{
	list_node* head;
	list_node* tail;
}link_list;
//自定义tcp
typedef struct define_tcp
{
	struct timeval timeout;//定时器
	link_list* queue_init;//初始化过的任务队列
	link_list* queue_uninit;//未初始化的任务队列
	int task_num;//任务总数
	int task_done;//已完成任务数
	int task_undone;//未完成任务数
}define_tcp;
//定义发送线程参数结构
typedef struct send_pthread_arg
{
	int udp_socket;
	int is_ack;//发送验证位
	struct sockaddr_in server_addr;
	int s_fd;//文件fd
	char filename[20];//文件名称,
}send_pthread_arg;
//定义写入文件线程参数
typedef struct write_file_msg
{
	client_info cli_fileid;
	socklen_t addr_len;
	struct sockaddr_in c_addr;
	struct write_file_msg* next;//感觉这个可以不要，明天有空改改
}write_file_msg;
char client_fd[10];//记录本地id
define_tcp d_tcp;
//将服务端发过来的客户端地址 存入本地文件结构体
typedef struct write_file_arg
{
	int fd;
	int myself_sock;
	struct sockaddr_in myself_addr;
}write_file_arg;
//定义发送线程结构体
typedef struct send_heart_arg
{
	int client_fd;
	client_info client_id;
}send_heart_arg;
int con_var=0;//条件变量
link_list* add_friend;//添加好友的链表
//链表操作函数
//初始化链表
link_list* init_link_list()
{
	link_list* l_list=(link_list*)malloc(1*sizeof(link_list));
	while(l_list==NULL)
	{
		l_list=(link_list*)malloc(1*sizeof(link_list));
	}
	l_list->head=NULL;
	l_list->tail=NULL;
	return l_list;
}
//将元素放入链表
void push_to_link_list(link_list* l_list,list_node* ptr_new)
{
	if(l_list->head==NULL && l_list->tail==NULL)
	{
		l_list->head=ptr_new;
		l_list->tail=ptr_new;
	}
	else
	{
		l_list->tail->next=ptr_new;
	}
	l_list->tail=ptr_new;
	ptr_new=NULL;
}
//判断链表是否为空
int is_list_empty(link_list* l_list)
{
	if(l_list->head==NULL && l_list->tail==NULL)
	{
		return 1;
	}
	return 0;
}
//出链表
list_node* pop_from_link_list(link_list* l_list)
{
	list_node* ptr_node=NULL;
	if(!is_list_empty(l_list))
	{
		ptr_node=l_list->head;
		l_list->head=l_list->head->next;
		if(l_list->head==NULL)
		{
			l_list->tail=NULL;
		}
	}
	return ptr_node;
}
//删除链表中的元素即删除好友
void delete_list_element(link_list* l_list,char* delete_frendid)
{
	list_node* ptr_cur=l_list->head;
	list_node* temp=l_list->head;
    list_node* ptr_delete=NULL;
	if(!is_list_empty(l_list))
	{
		while(ptr_cur!=NULL)
		{
			if(strcmp(l_list->head->cli_friend_id,delete_frendid)==0)
			{
				list_node* ptr_temp=l_list->head;
				l_list->head=l_list->head->next;
				free(ptr_temp);
				ptr_temp=NULL;
			}
			if(strcmp(ptr_cur->cli_friend_id,delete_frendid)==0)
			{				  
				ptr_delete=ptr_cur;
				temp->next=ptr_cur->next;					
				free(ptr_delete);
				ptr_delete=NULL;
			}
			else
			{
				temp=ptr_cur;
				l_list->tail=temp;//防止链表尾节点变成野指针
			}
			ptr_cur=ptr_cur->next;				
		}			
	}
}
//查找即查找好友
list_node* find_friendid(link_list* l_list,char* find_friend_id)
{
	list_node* ptr_cur=NULL;
	for(ptr_cur=l_list->head;ptr_cur!=NULL;ptr_cur=ptr_cur->next)
	{
		if(strcmp(ptr_cur->cli_friend_id,find_friend_id)==0)
		{
			return ptr_cur;
		}
	}
	return ptr_cur;
}
//文件操作相关函数
int open_file(const char* filename)
{
	int fd=open(filename,OPEN_CWD_FLAGS,0777);
	if(fd==-1)
	{
		if(errno==EEXIST)
		{
			fd=open(filename,O_RDWR);
		}
		else
		{
			perror("open error:");
			exit(-1);
		}
	}
	else
	{
		fd=open(filename,O_RDWR);
	}
	return fd;
}
//菜单函数
void menu_fun()
{
	printf("1代表发送数据\n");
	printf("2代表发送文件\n");
	printf("3添加好友\n");
	printf("4代表删除好友\n");
}
//初始化服务端地址结构
struct sockaddr_in init_sever_addr(int port)
{
	struct sockaddr_in sever_addr;
	bzero(&sever_addr,sizeof(sever_addr));
	sever_addr.sin_family=AF_INET;
	sever_addr.sin_port=htons(port);
	sever_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	return sever_addr;
}
//初始化tcp套接字并绑定
int init_tcp_socket(struct sockaddr_in server_addr)
{
	int client_fd=socket(AF_INET,SOCK_STREAM,0);//定义客户端套接字，客户端套接字为tcp套接字
	while(client_fd==-1)
	{
		while(1)
		{
			printf("连接失败 是否重新连接 重新连接 请输入0 不连接 请输入1\n");
			int is_again;
			scanf("%d",&is_again);
			if(is_again==0)
			{
				client_fd=socket(AF_INET,SOCK_STREAM,0);
			}
			if(is_again==1)
			{
				perror("tcp_socket error:");
				exit(-1);
			}
		}
	}
	if(connect(client_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1)
	{
		while(1)
		{
			printf("连接失败 是否重新连接 重新连接 请输入0 不连接 请输入1\n");
			int is_again;
			scanf("%d",&is_again);
			if(is_again==0)
			{
				client_fd=socket(AF_INET,SOCK_STREAM,0);
			}
			if(is_again==1)
			{
				perror("tcp_socket error:");
				exit(-1);
			}
		}
	}
	return client_fd;
}
//初始化udp套接字并绑定
int init_udp_socket(struct sockaddr_in udp_addr)
{
	int udp_socket=socket(PF_INET,SOCK_DGRAM,0);
	if(udp_socket==-1)
	{
		perror("udp_socket error:");
		exit(-1);
	}
	if(bind(udp_socket,(struct sockaddr*)&udp_addr,sizeof(udp_addr))==-1)
	{
		perror("udp_bind error:");
		exit(-1);
	}
	return udp_socket;
}
//初始化自定义tcp
define_tcp* init_define_tcp()
{
	define_tcp* d_tcp=(define_tcp*)malloc(1*sizeof(define_tcp));
	while(d_tcp==NULL)
	{
		d_tcp=(define_tcp*)malloc(1*sizeof(define_tcp));
	}
	d_tcp->queue_uninit=NULL;
	d_tcp->queue_init=NULL;
	d_tcp->task_num=0;
	d_tcp->task_undone=0;
	d_tcp->task_done=0;
	d_tcp->timeout.tv_sec=60;
	d_tcp->timeout.tv_usec=0;
	return d_tcp;
}
//添加好友
int add_friends(link_list* add_f)
{
	write_file_msg add_fr;
	int fd=open_file("zjr");//打开存储好友信息的文件
	int add_fd=open_file("my_friend_file");
	//先将已连接的好友输出 供选择
	while(1)
	{
		int ret=read(fd,&add_fr,sizeof(add_fr));
		while(ret==-1)
		{
			ret=read(fd,&add_fr,sizeof(add_fr));
		}
		printf("%s\n",add_fr.cli_fileid.cli_id);
		if(ret==0)//读完退出循环
		{
			break;
		}
	}
	lseek(fd,0,SEEK_SET);//将文件内部针移到文件开头
	char add_friendid[20];
	printf("请输入要添加的好友id:\n");
	gets(add_friendid);//输入要添加的好友号码
	while(1)
	{
		int ret=read(fd,&add_fr,sizeof(add_fr));
		if(strcmp(add_fr.cli_fileid.cli_id,add_friendid)==0)
		{
		    list_node ptr_new;
			strcpy(ptr_new.cli_friend_id,add_friendid);
			ptr_new.next=NULL;
			push_to_link_list(add_f,&ptr_new);
			//printf("%s\n",ptr_new->cli_friend_id);
			lseek(add_fd,0,SEEK_END);
			write(add_fd,(void*)&ptr_new,sizeof(list_node));
			return;
		}
		if(ret==0)
		{
			printf("没有此号码\n");
			return;
		}
	}
	close(add_fd);
}
//删除好友
void delete_friends(link_list* delete_f)
{
	list_node ptr_temp;
	int delete_fd=open_file("my_friend_file");
	//先将已有的好友输出 供选择
	while(1)
	{
		int ret=read(delete_fd,(void*)&ptr_temp,sizeof(list_node));
		while(ret==-1)
		{
			ret=read(delete_fd,(void*)&ptr_temp,sizeof(list_node));
		}
		printf("%s\n",ptr_temp.cli_friend_id);
		if(ret==0)//读完退出循环
		{
			break;
		}
	}
	lseek(delete_fd,0,SEEK_SET);//将文件内部针移到文件开头
	char de_friendid[20];
	printf("请输入要删除的好友id:\n");
	gets(de_friendid);//输入要删除的好友号码
	list_node ptr_cur;
	int tem_fd=open_file("temp");//创建一个临时文件,存储不要删除的内容
	while(1)
	{
		int ret=read(delete_fd,(void*)&ptr_cur,sizeof(ptr_cur));
		while(ret==-1)
		{
			ret=read(delete_fd,(void*)&ptr_cur,sizeof(ptr_cur));
		}
		if(strcmp(ptr_cur.cli_friend_id,de_friendid)!=0)
		{
			write(tem_fd,(void*)&ptr_cur,sizeof(ptr_cur));
		}
		if(ret==0)
		{
			close(delete_fd);
			break;
		}
	}
	int ret=unlink("my_friend_file");//删除文件my_friend_file
	int de_fd=open_file("my_friend_file");
	while(1)
	{
		int ret=read(tem_fd,(void*)&ptr_cur,sizeof(ptr_cur));
		while(ret==-1)
		{
			ret=read(tem_fd,(void*)&ptr_cur,sizeof(ptr_cur));
		}
		write(de_fd,(void*)&ptr_cur,ret);
		if(ret==0)
		{
			close(de_fd);
			break;
		}
	}
	unlink("temp");	
}
//定义发送线程
void* send_pthread(void* arg)
{
	send_pthread_arg pthread_arg=*(send_pthread_arg*)arg;
	send_pack pack;
	int udp_fd=pthread_arg.udp_socket;
	define_tcp* d_tcp=init_define_tcp();
	menu_fun();
	while(1)
	{
		printf("请输入所需操作:\n");
		scanf("%d",&con_var);
		getchar();//清空缓存
		if(con_var==1)
		{
			pack.type=DATA;
			pack.is_ack=0;//发送标志位为0
			strcpy(pack.meg_send.cli_id,client_fd);
			gets(pack.meg_recv.cli_id);
			gets(pack.msg_data);
			strcpy(pack.filename,pthread_arg.filename);
		    int ret=sendto(udp_fd,(void*)&pack,sizeof(pack),0,(struct sockaddr*)&(pthread_arg.server_addr),sizeof(pthread_arg.server_addr));
		    if(ret==-1)
		    {
		    	ret=sendto(udp_fd,(void*)&pack,sizeof(pack),0,(struct sockaddr*)&(pthread_arg.server_addr),sizeof(pthread_arg.server_addr));
		    }	
		    printf("ok\n");
        }
    	else if(con_var==2)
	    {
			printf("请输入要发送的文件名:\n");
			//getchar();
			char buf[20];
			gets(buf);
			pthread_arg.s_fd=open_file(buf);
			strcpy(pack.meg_send.cli_id,client_fd);
			gets(pack.meg_recv.cli_id);
			while(1)
			{
				int ret_read=read(pthread_arg.s_fd,pack.msg_data,sizeof(buf)-1);
				if(ret_read<0)
				{
					printf("读取文件失败\n");
					break;
				}
				if(ret_read==0)
				{
					printf("dead end\n");
					break;
				}
			    int  ret=sendto(pthread_arg.udp_socket,(void*)&pack,sizeof(pack),0,(struct sockaddr*)&pthread_arg.server_addr,sizeof(pthread_arg.server_addr));
			    while(ret==-1)
			    {
			    	ret=sendto(pthread_arg.udp_socket,(void*)&pack,sizeof(pack),0,(struct sockaddr*)&(pthread_arg.server_addr),sizeof(pthread_arg.server_addr));
			    }
		 	}	
			con_var=1;
        }
        else if(con_var==3)
        {
        	add_friends(add_friend);
        }
        else if(con_var==4)
        {
        	delete_friends(add_friend);
        }
	}	
}
//定义接收线程接收到数据需要回发确认
void* recv_pthread(void* arg)
{
	send_pthread_arg recv_pthread=*(send_pthread_arg*)arg;
	int udp_socket=recv_pthread.udp_socket;
	send_pack recv_pack;
	while(1)
	{
		bzero(&recv_pack,sizeof(recv_pack));
		int recv_len=recvfrom(udp_socket,(void*)&recv_pack,sizeof(recv_pack),0,NULL,NULL);
		if(recv_len>0)
		{
			if(con_var==1)
			{
				if(recv_pack.is_ack=0)
				{
					recv_pack.is_ack=1;
					sendto(udp_socket,(void*)&recv_pack,sizeof(recv_pack),0,(struct sockaddr*)&(recv_pthread.server_addr),sizeof(recv_pthread));
				}
				else
				{
					struct timeval timeout;
					timeout.tv_sec=0;
					timeout.tv_usec=0;
					d_tcp.timeout=timeout;
					setitimer(ITIMER_REAL,(struct itimerval*)&d_tcp.timeout,NULL);//收到确认信号马上终止定时	
				}
				printf("%s\n",recv_pack.msg_data);
			}
			else if(con_var==2)
			{
				char buf[1]={' '};			
				while(1)
				{
					int recv_len=recvfrom(udp_socket,(void*)&recv_pack,sizeof(recv_pack),0,NULL,NULL);
					if(recv_len>0)
					{
						int fd=open_file(recv_pack.filename);
						lseek(fd,recv_len-1,SEEK_SET);
						write(fd,buf,1);
						write(fd,recv_pack.msg_data,sizeof(recv_pack.msg_data));
					}
					if(recv_len<=0)
					{
						break;
					}
					printf("%s\n",recv_pack.msg_data);
				}
				con_var=0;	
			}	
		} 
	}		
}
//默认刚连接上就发送心跳包
//定义发送心跳包线程
void* send_heart(void* arg)
{
	if(con_var==0)
	{
		send_heart_arg send_heart=*(send_heart_arg*)arg;
		send_pack pack;
		int count=0;
		while(1)
		{	
			pack.type=HEARD;
			strcpy(pack.meg_send.cli_id,send_heart.client_id.cli_id);
			send(send_heart.client_fd,(void*)&pack,sizeof(pack),0);
			sleep(3);
			count++;
			if(count>5)
				break;
		}	
	}	
}
//写入文件的线程
void* write_file_pthread(void* arg)
{
	write_file_arg wr_file_arg=*(write_file_arg*)arg;
	write_file_msg recv_arg;
	while(1)
	{
		int ret=recv(wr_file_arg.myself_sock,(void*)&recv_arg,sizeof(write_file_msg),0);
		if(ret>0)
		{
			//将信息写入文件
			write(wr_file_arg.fd,(void*)&recv_arg,sizeof(recv_arg));
		}
		
	}	
}

//客户端发送消息给服务端
void client_send_message(struct sockaddr_in server_addr,int client_fd,char* cli_id,int fd,int s_fd,const char* filename)
{
	int udp_socket;
    init_define_tcp(d_tcp);
	send_pack p_send;
	bzero(&p_send,sizeof(send_pack));
	p_send.type=DATA;
	strcpy(p_send.meg_send.cli_id,cli_id);
	send(client_fd,(void*)&p_send,sizeof(p_send),0);
	struct sockaddr_in udp_addr;
	bzero(&udp_addr,sizeof(udp_addr));
	struct sockaddr_in my_addr;
	bzero(&my_addr,sizeof(my_addr));
	write_file_arg recv_file_arg;
	bzero(&recv_file_arg,sizeof(recv_file_arg));
	int ret=recv(client_fd,(void*)&udp_addr,sizeof(udp_addr),0);
	if(ret>0)//证明收到服务端回发的地址
	{
		udp_socket=init_udp_socket(udp_addr);
		pthread_t pthreadid[4];
		write_file_arg wr_file_arg;
	    bzero(&wr_file_arg,sizeof(wr_file_arg));
	    wr_file_arg.fd=fd;
	    wr_file_arg.myself_addr=my_addr;
	    wr_file_arg.myself_sock=client_fd;
		send_pthread_arg send_recv_arg;
		bzero(&send_recv_arg,sizeof(send_recv_arg));
		send_recv_arg.udp_socket=udp_socket;
		send_recv_arg.is_ack=0;
		send_recv_arg.server_addr=server_addr;
	    strcpy(send_recv_arg.filename,filename);
	    send_recv_arg.s_fd=s_fd;
	    send_heart_arg send_heart;
	    send_heart.client_fd=client_fd;
	    strcpy(send_heart.client_id.cli_id,cli_id);
		int ret=pthread_create(&pthreadid[0],NULL,send_pthread,(void*)&send_recv_arg);
		while(ret==-1)
		{
			ret=pthread_create(&pthreadid[0],NULL,send_pthread,(void*)&send_recv_arg);
		}
	    ret=pthread_create(&pthreadid[1],NULL,recv_pthread,(void*)&send_recv_arg);
		while(ret==-1)
		{
			ret=pthread_create(&pthreadid[1],NULL,recv_pthread,(void*)&send_recv_arg);
		}
		ret=pthread_create(&pthreadid[2],NULL,write_file_pthread,(void*)&wr_file_arg);
		while(ret==-1)
		{
			ret=pthread_create(&pthreadid[2],NULL,write_file_pthread,(void*)&wr_file_arg);
		}
		pthread_join(pthreadid[0],NULL);
		pthread_join(pthreadid[1],NULL);
		pthread_join(pthreadid[2],NULL);
	}
}

int main(int argc, char const *argv[])
{
	int s_fd=0;
	char cli_id[10];
	printf("请输入账号:\n");
	gets(cli_id);
	link_list* l_list=init_link_list();
	add_friend=init_link_list();
	struct sockaddr_in sever_addr=init_sever_addr(5678);
	int client_fd=init_tcp_socket(sever_addr);
	int fd=open_file("zjr");
	client_send_message(sever_addr,client_fd,cli_id,fd,s_fd," ");
	return 0;
}