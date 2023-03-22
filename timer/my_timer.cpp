#include "my_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst(int cap=100) ： capacity(cap), cur_size(0)
{
    array = new util_timer *[capacity];
    for (int i=0;i<capacity;++i)
    {
        array[i] = NULL;
    }
  
}

sort_timer_lst::~sort_timer_lst()
{
    for (int i = 0; i < cur_size; ++i)
    {
        delete array[i];
    }
    delete [] array;
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
        return;
    if (cur_size >= capacity)
        resize();
    int hole = cur_size++;
    int parent = 0;
    for ( ; hole > 0; hole = parent)
    {
        parent = (hole-1) / 2;
        if ( array[parent]->expire <= timer->expire)
            break;
        array[hole] = array[parent];
    }
    array[hole] = timer;
}

void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
        return;
    timer->cb_func = NULL;
}

void sort_timer_lst::adjust_timer(util_timer *timer)
{
     if (!timer)
    {
        return;
    }
}

void sort_timer_lst::pop_timer()
{
    if (empty())
        return;
    if (array[0])
    {
        delete array[0];
        array[0] = array[--cur_size];
        percolate_down(0);
    }
}

void sort_timer_lst::tick()
{
    util_timer *tmp = array[0];
    time_t cur = time(NULL);
    while (!empty())
    {
        if (!tmp)
            break;
        if (tmp->expire > cur)
            break;
        
        if (array[0]->cb_func)
            array[0]->cb_func( array[0]->user_data );

        pop_timer();
        tmp = array[0];

    }
}

void sort_timer_lst::percolate_down(int hole)
{
    util_timer *tmp = array[hole];
    int child = 0;
    for ( ; ((hole*2 + 1) <= (cur_size-1)); hole=child)
    {
        child = hole*2 +1;
        if (child < (cur_size-1) && (array[child + 1]->expire < array[child]-expire) )
        {
            ++child;
        }
        if ( array[child]->expire < tmp->expire)
            array[hole] = array[child];
        else   
            break;
        
    }
    array[hole] = tmp;
}

void sort_timer_lst::resize()
{
    util_timer ** tmp = new util_timer * [2*capacity];
    for (int i=0; i < 2*capacity; ++i)
    {
        tmp[i] = NULL;
    }
    capacity = 2 *capacity;
    for (int i=0; i <cur_size; ++i)
    {
        tmp[i] = array[i];
    }
    delete [] array;
    array = tmp;
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    //fcntl可以获取/设置文件描述符性质
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，是否选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    //如果对描述符socket注册了EPOLLONESHOT事件，
    //那么操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次。
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    //最小的时间单位为5s
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;


class Utils;
//定时器回调函数:从内核事件表删除事件，关闭文件描述符，释放连接资源
void cb_func(client_data *user_data)
{
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    //删除非活动连接在socket上的注册事件
    close(user_data->sockfd);
    //减少连接数
    http_conn::m_user_count--;
}
