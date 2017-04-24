#include "define_all_header.h"
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
//定义链表节点
typedef struct list_node
{
	client_info cli_friend_id;
	socklen_t client_addr_len;
	struct sockaddr_in c_addr;
	struct list_node* next;
	int count;
	int client_fd;
}list_node;
//定义链表
typedef struct link_list
{
	list_node* head;
	list_node* tail;
}link_list;
//定义发送数据报结构
typedef struct send_pack
{
	type_pack type;
	client_info meg_send;//发送方id
	client_info meg_recv;//接收方id
	int is_ack;//发送验证位
	char msg_data[1024];//发送的数据
	char filename[20];
} send_pack;
//定义心跳包处理函数结构体
typedef struct header_pthread_arg
{
	link_list* l_list;
	fd_set* work_set;
	int* max_fd;
}header_pthread_arg;
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
//查找
list_node* find_friendid(link_list* l_list,char* find_friend_id)
{
	list_node* ptr_cur=NULL;
	for(ptr_cur=l_list->head;ptr_cur!=NULL;ptr_cur=ptr_cur->next)
	{
		if(strcmp(ptr_cur->cli_friend_id.cli_id,find_friend_id)==0)
		{
			return ptr_cur;
		}
	}
	return ptr_cur;
}
//从链表中删除元素
void list_delete(int client_fd,link_list* l_list)
{
	if(!is_list_empty(l_list))
	{
		list_node* delete_node=l_list->head;
		list_node* temp=NULL;
		while(delete_node!=NULL)
		{
			if(delete_node->client_fd==client_fd)
			{
				temp->next=delete_node->next;
				delete_node->next=NULL;
				free(delete_node);
				delete_node=NULL;
			}
			else
			{
				temp=delete_node;
			}
			temp=temp->next;
		}
	}
}
//出链表
list_node* pop_link_list(link_list* l_list)
{
	list_node* temp=NULL;
	if(!is_list_empty(l_list))
	{
		temp=l_list->head;
		l_list->head=l_list->head->next;
		if(l_list->head==NULL)
		{
			l_list->tail==NULL;
		}
	}
	return temp;
}
//初始化自身地址结构
struct sockaddr_in init_sever_addr(int port)
{
	struct sockaddr_in sever_addr;
	bzero(&sever_addr,sizeof(sever_addr));
	sever_addr.sin_family=AF_INET;
	sever_addr.sin_port=htons(port);
	sever_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	return sever_addr;
}
//改变套接字状态
void get_new_status(int socket_fd)
{
	int status_flag=fcntl(socket_fd,F_GETFL);
	status_flag=status_flag|O_NONBLOCK;
	fcntl(socket_fd,status_flag);
}
//初始化tcp套接字并绑定
int init_tcp_socket(struct sockaddr_in sever_addr)
{
	int listen_fd=socket(AF_INET,SOCK_STREAM,0);//定义客户端套接字，客户端套接字为tcp套接字
	if(listen_fd==-1)
	{
		perror("listen_fd error:");
		//exit(-1);
	}
	if(bind(listen_fd,(void*)&sever_addr,sizeof(sever_addr))==-1)
	{
		perror("bind error:");
	}
	if(listen(listen_fd,10)==-1)
	{
		perror("listen error:");
	}
	int isreuse=1;
	setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,(void*)isreuse,sizeof(listen_fd));
	return listen_fd;
}
//初始化udp套接字并绑定
int init_udp_socket(struct sockaddr_in sever_addr)
{
	int udp_socket=socket(PF_INET,SOCK_DGRAM,0);
	if(udp_socket==-1)
	{
		perror("udp_socket error:");
		exit(-1);
	}
	if(bind(udp_socket,(struct sockaddr*)&sever_addr,sizeof(sever_addr))==-1)
	{
		perror("udp_bind error:");
		exit(-1);
	}
	return udp_socket;
}
//心跳包处理线程
void* header_work(void* arg)
{
	printf("header ok\n");
	header_pthread_arg* header_work=(header_pthread_arg*)arg;
	link_list* header_list=header_work->l_list;
	while(1)
	{
		list_node* ptr_cur=NULL;
		for(ptr_cur=header_list->head;ptr_cur!=NULL;ptr_cur=ptr_cur->next)
		{
			int free_fd=ptr_cur->client_fd;
			close(free_fd);
			FD_CLR(free_fd,header_work->work_set);
			if(free_fd=*(header_work->max_fd))
			{
				while(FD_ISSET(*(header_work->max_fd),header_work->work_set)==0)
				{
					*(header_work->max_fd)--;
				}
				list_delete(free_fd,header_list);//从链表中删除该记录
			}
			else if(ptr_cur->count<5 && ptr_cur->count>=0)
			{
				ptr_cur->count++;
			}
		}
		sleep(3);
	}
}
//服务端消息转发
void sever_send(int udp_fd,int listen_fd,link_list* l_list,link_list* send_list)
{
	int max=0;
	fd_set read_set;
	struct timeval timeout;
	int fd_num;//状态变化的套接字数量
	max=listen_fd>udp_fd?listen_fd:udp_fd;
	header_pthread_arg header_arg;
	bzero(&header_arg,sizeof(header_arg));
	header_arg.max_fd=&max;
	header_arg.l_list=l_list;
	header_arg.work_set=&read_set;
	while(1)
	{
		FD_ZERO(&read_set);
		FD_SET(listen_fd,&read_set);
		FD_SET(udp_fd,&read_set);
		timeout.tv_sec=0;
		timeout.tv_usec=100;
		fd_num=select(max+1,&read_set,NULL,NULL,&timeout);
		if(fd_num<=0)
		{
			continue;
		}
		if(fd_num>0)
		{
			if(FD_ISSET(listen_fd,&read_set))
			{
				int client_fd;
				struct sockaddr_in client_addr;
				bzero(&client_addr,sizeof(client_addr));
				socklen_t client_addr_len=sizeof(client_addr);
				client_fd=accept(listen_fd,(struct sockaddr*)&client_addr,&client_addr_len);
				if(client_fd==-1)
				{
					perror("accept error:");
				}
				if(client_fd>0)
				{
					send_pack msg_arg;
					bzero(&msg_arg,sizeof(msg_arg));
					int ret=recv(client_fd,(void*)&msg_arg,sizeof(msg_arg),0);
					if(ret>0)//证明收到消息了
					{
						if(msg_arg.type==DATA)
						{
							list_node* ptr_new=(list_node*)malloc(1*sizeof(list_node));
							while(ptr_new==NULL)
							{
								ptr_new=(list_node*)malloc(1*sizeof(list_node));
							}
							strcpy(ptr_new->cli_friend_id.cli_id,msg_arg.meg_send.cli_id);
							ptr_new->client_addr_len=client_addr_len;
							ptr_new->c_addr=client_addr;
							ptr_new->client_fd=client_fd;
							ptr_new->next==NULL;
							push_to_link_list(l_list,ptr_new);
							push_to_link_list(send_list,ptr_new);
							send(client_fd,(void*)&client_addr,sizeof(client_addr),0);
							list_node* ptr_cur=send_list->head;
							list_node* temp=send_list->head;//暂时保存头结点
							for(ptr_cur=send_list->head;ptr_cur!=NULL;ptr_cur=ptr_cur->next)
							{
								list_node send_node=*(pop_link_list(send_list));
								//printf("2\n");
								//因为发送的不是地址结构了 所以sizeof要变成要发送的节点的大小
								send(client_fd,(void*)&send_node,sizeof(send_node)-2*sizeof(int),0);
							}
							send_list->head=temp;//将头结点还回去
						}
						if(msg_arg.type==HEARD)
						{
							pthread_t pthreadid;
							int ret=pthread_create(&pthreadid,NULL,header_work,(void*)&header_arg);
							while(ret==-1)
							{
								ret=pthread_create(&pthreadid,NULL,header_work,(void*)&header_arg);
							}
							printf("header ok\n");
							list_node* ptr_new=find_friendid(l_list,msg_arg.meg_send.cli_id);
							ptr_new->count=0;
							pthread_join(pthreadid,NULL);
						}
					}					
				}
			}
			if(FD_ISSET(udp_fd,&read_set))
			{
				send_pack msg;
				bzero(&msg,sizeof(msg));
				int recv_byte=recvfrom(udp_fd,(void*)&msg,sizeof(msg),0,NULL,NULL);
				if(recv_byte>0)
				{
					list_node* find_node=find_friendid(l_list,msg.meg_recv.cli_id);
					if(find_node!=NULL)
					{
						sendto(udp_fd,(void*)&msg,sizeof(msg),0,(struct sockaddr*)&find_node->c_addr,find_node->client_addr_len);
					}
				}
			}

		}
	}
}
int main(int argc, char const *argv[])
{
	link_list* l_list=init_link_list();
	link_list* send_list=init_link_list();
	struct sockaddr_in sever_addr=init_sever_addr(5678);
	int listen_fd=init_tcp_socket(sever_addr);
	int udp_fd=init_udp_socket(sever_addr);
	sever_send(udp_fd,listen_fd,l_list,send_list);
	return 0;
}