#include "timer.h"

sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst(){
    util_timer *temp = head;
    while(temp){
        head = temp->next;
        delete temp;
        temp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer){
    if (timer == NULL){
        return;
    }
    if (head == NULL){
        head = timer;
        tail = timer;
        return;
    }
    if (timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer){
    if (timer == NULL){
        return;
    }
    //尾结点不用后调了
    if (timer == tail){
        return;
    }
    //新的时间仍然小于next结点的时间
    else if(timer->expire < timer->next->expire){
        return;
    }
    else if (timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, head);
    }
}

void sort_timer_lst::del_timer(util_timer *timer){
    if (timer == NULL){
        return;
    }
    if ((head == timer) && (tail == timer)){
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (head == timer){
        head->next->prev = NULL;
        head = head->next;
        delete timer;
        return;
    }
    if (tail == timer){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp){
        if (timer->expire < tmp->expire){
            prev->next = timer;
            timer->prev = prev;
            timer->next = tmp;
            tmp->prev = timer;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_timer_lst::tick(){
    if (head == NULL){
        return;
    }
    time_t cur_time = time(NULL);
    util_timer *tmp = head;
    while(tmp){
        if (cur_time < tmp->expire){
            return;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
        printf("delete a timer\n");
    }
}

int sort_timer_lst::size(){
    util_timer *tmp = head;
    int count = 0;
    while(tmp){
        count++;
        tmp = tmp->next;
    }
    return count;
}

void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}