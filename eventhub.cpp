#include "eventhub.h"
#include <malloc.h>
HZ_BOOL Enabled = HZ_TRUE;
HZ_BOOL initFlag = HZ_FALSE;

map<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>> sub_event_list;
map<HI_SUBSCRIBER_S*,queue<HI_EVENT_S*>> sub_pre_events;
list<HI_EVENT_S*> event_queue; //历史事件表
HI_EVENT_S event;
pthread_mutex_t _mutexQueue;
pthread_mutex_t _historyMutex = PTHREAD_MUTEX_INITIALIZER;
void * handle_message(/*HI_SUBSCRIBER_S* sub,HI_EVENT_S *e*/void* arg)
{
    HI_SUBSCRIBER_S *sub = (HI_SUBSCRIBER_S *)arg;

    while (1) {


        if(sub_pre_events[sub].size() > 0)
        {
            if(Enabled == HZ_FALSE || initFlag == HZ_FALSE)
            {
                usleep(1000*10);
                continue;
            }

            HI_EVENT_S * e = sub_pre_events[sub].front();

            list<HI_EVENT_ID>::iterator it;
            for(it = sub_event_list[sub].begin(); it != sub_event_list[sub].end(); it++)
            {
                if((*it) == e->EventID)
                {
                    sub->HI_EVTHUB_EVENTPROC_FN_PTR(e,NULL);
                }
            }
            pthread_mutex_lock(&_mutexQueue);
//            if(e != NULL)
//            {
//                free(e);
//                e =NULL;
//            }
            sub_pre_events[sub].pop();
            pthread_mutex_unlock(&_mutexQueue);
        }else{
            usleep(1000*1000);
            malloc_trim(0);//空闲时回收内存碎片
        }

    }
    return NULL;
}

EventHub::EventHub()
{

}
EventHub::~EventHub()
{

}

int EventHub::EVTHUB_Init()
{
    pthread_mutex_init(&_mutexSubscriber, nullptr);
    pthread_mutex_init(&_mutexQueue, nullptr);
    pthread_mutex_init(&_mutexPublish, nullptr);
    pthread_mutex_init(&_mutexConn, nullptr);


    pthread_mutex_lock(&_mutexConn);
    initFlag = HZ_TRUE;
    pthread_mutex_unlock(&_mutexConn);

    return 0;
}

int EventHub::EVTHUB_Deinit()
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited,no need to deinit\n");
        return 0;
    }

    if (pthread_mutex_lock(&_mutexConn) != 0){
        fprintf(stdout, "lock error!\n");
        return -100;
    }

    Enabled = HZ_FALSE;

    while (!event_queue.empty())//清空历史事件
    {
        free(event_queue.front());
        event_queue.pop_front();
    }

    plist.erase(plist.begin(),plist.end());

    //结束所有订阅者线程,释放所有订阅者指针
    for(map<HI_SUBSCRIBER_S*,pthread_t>::iterator it = sub_threads.begin() ;it != sub_threads.end();)
    {
        printf("will free sub : %lu\n",it->second);
        pthread_cancel(it->second);
        pthread_join(it->second,NULL);
        HI_SUBSCRIBER_S * s = NULL;
        s = it->first;
        if(s != NULL)
        {
            //printf("will free sub\n");
            free(s);
            s = NULL;
        }
        sub_threads.erase(it++);


    }

    map<HI_SUBSCRIBER_S*,queue<HI_EVENT_S*>>::iterator it = sub_pre_events.begin() ;

    //sub_threads.erase(sub_threads.begin(),sub_threads.end());

    for (it; it != sub_pre_events.end(); ) {
        while (it->second.size() > 0) {
            it->second.pop();
        }
        sub_pre_events.erase( it++);
    }

    printf("will deinit: %d  %d  %d\n",sub_threads.size(),sub_pre_events.size(),event_queue.size());
    malloc_trim(0);
    pthread_mutex_unlock(&_mutexConn);
    printf("has deinited\n");
    return 0;
}

int EventHub::HZ_EVTHUB_Register(HI_EVENT_ID EventID)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    //printf("registed lock\n");
    if (pthread_mutex_lock(&_mutexPublish) != 0){
        fprintf(stdout, "lock error!\n");
    }

    vector<HI_EVENT_ID>::iterator it;
    for(it = plist.begin(); it != plist.end() ; it++)
    {
        if((*it) == EventID)
        {
            pthread_mutex_unlock(&_mutexPublish);
            printf("has registered\n");
            return 0;
        }
    }
    plist.push_back(EventID);
    pthread_mutex_unlock(&_mutexPublish);
    //printf("registed unlock\n");
    return 0;
}

int EventHub::HZ_EVTHUB_UnRegister(HI_EVENT_ID EventID)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    if (pthread_mutex_lock(&_mutexPublish) != 0){
        fprintf(stdout, "lock error!\n");
    }
    vector<HI_EVENT_ID>::iterator it;
    for(it = plist.begin(); it != plist.end() ; it++)
    {
        if((*it) == EventID)
        {
            plist.erase(it);
            pthread_mutex_unlock(&_mutexPublish);
            printf("has unregistered\n");
            return 0;
        }
    }

    pthread_mutex_unlock(&_mutexPublish);
    printf("this id has not registered\n");
    return 0;
}

int EventHub::HZ_EVTHUB_CreateSubscriber(HI_SUBSCRIBER_S *pstSubscriber, HI_MW_PTR *ppSubscriber)
{

    if(*ppSubscriber != NULL)
    {
        printf("create subcriber error!\n");
        return -200;
    }



    pthread_mutex_lock(&_mutexSubscriber);

    HI_SUBSCRIBER_S *sub_c = (HI_SUBSCRIBER_S *)malloc(sizeof(HI_SUBSCRIBER_S));
    memset(sub_c,0,sizeof(HI_SUBSCRIBER_S));
    memcpy(sub_c,pstSubscriber,sizeof(HI_SUBSCRIBER_S));

    *ppSubscriber = sub_c;

    list<HI_EVENT_ID> e_list;
    queue<HI_EVENT_S*> pre_e_list;
    sub_event_list.insert(pair<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>(sub_c,e_list));
    sub_pre_events.insert(pair<HI_SUBSCRIBER_S*,queue<HI_EVENT_S*>>(sub_c,pre_e_list));
    //sub_pre_events[sub_c] = pre_e_list;

    pthread_t sub_t;
    if(pthread_create(&sub_t,NULL,handle_message,sub_c) != 0)
    {
        pthread_mutex_unlock(&_mutexSubscriber);
        printf("create sub thread failed\n");
        return -300;
    }

    sub_threads.insert(pair<HI_SUBSCRIBER_S*,pthread_t>(sub_c,sub_t));


    printf("will create a suber %ld \n",*ppSubscriber);
    pthread_mutex_unlock(&_mutexSubscriber);
    //printf("creat a new subcriber, func pointer: %ld \n",sub_c->HI_EVTHUB_EVENTPROC_FN_PTR);
    return 0;
}

int EventHub::HZ_EVTHUB_DestroySubscriber(HI_SUBSCRIBER_S *pstSubscriber)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    //printf("will destroy suber: %ld",pstSubscriber);
    pthread_mutex_lock(&_mutexSubscriber);

    map<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>::iterator e_it;
    e_it = sub_event_list.find(pstSubscriber);
    if(e_it != sub_event_list.end())
    {
        sub_event_list.erase(e_it);
    }

    map<HI_SUBSCRIBER_S*,queue<HI_EVENT_S*>>::iterator q_it;
    q_it = sub_pre_events.find(pstSubscriber);
    if(q_it != sub_pre_events.end())
    {
        sub_pre_events.erase(q_it);
    }

    map<HI_SUBSCRIBER_S*,pthread_t>::iterator t_it;
    t_it = sub_threads.find(pstSubscriber);
    if(t_it != sub_threads.end())
    {
        //printf("will canacel thread:%lu\n", t_it->second);
        pthread_cancel(t_it->second);
        pthread_join(t_it->second,NULL);

    }

    if(pstSubscriber != NULL)
    {
        //printf("will free sub\n");
        free(pstSubscriber);
        pstSubscriber = NULL;
    }

    pthread_mutex_unlock(&_mutexSubscriber);
    printf("remove a subcriber\n");
    return 0;
}

int EventHub::HZ_EVTHUB_Subscribe(HI_MW_PTR pSubscriber, HI_EVENT_ID EventID)
{

    pthread_mutex_lock(&_mutexSubscriber);
    if(pSubscriber == NULL)
    {

        pthread_mutex_unlock(&_mutexSubscriber);
        printf("Please create a subcriber first!\n");
        return -200;
    }
    //没有注册的ID能订阅么?
    HI_SUBSCRIBER_S*sub = (HI_SUBSCRIBER_S *)pSubscriber;
    map<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>::iterator it;
    it = sub_event_list.find(sub);
    if(it != sub_event_list.end())
    {
        sub_event_list[sub].push_back(EventID);
        pthread_mutex_unlock(&_mutexSubscriber);
        return 0;
    }else{
        pthread_mutex_unlock(&_mutexSubscriber);
        printf("subcriber has not create yet! \n");
        return -400;
    }

    return 0;
}

int EventHub::HZ_EVTHUB_UnSubscribe(HI_MW_PTR pSubscriber, HI_EVENT_ID EventID)
{
    pthread_mutex_lock(&_mutexSubscriber);

    HI_SUBSCRIBER_S *sub = (HI_SUBSCRIBER_S *)pSubscriber;
    map<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>::iterator it;
    it = sub_event_list.find(sub);
    if(it != sub_event_list.end())
    {
        sub_event_list[sub].remove(EventID);
        pthread_mutex_unlock(&_mutexSubscriber);
        return 0;
    }else{
        pthread_mutex_unlock(&_mutexSubscriber);
        printf("subcriber has not create yet! \n");
        return -400;
    }
}

int EventHub::HZ_EVTHUB_Publish(HI_EVENT_S *pEvent)
{

//    HI_EVENT_S * event = (HI_EVENT_S *) malloc(sizeof(HI_EVENT_S));
//    memcpy(event,pEvent,sizeof(HI_SUBSCRIBER_S));
    vector<HI_EVENT_ID>::iterator it;
    for(it = plist.begin(); it != plist.end() ; it++)
    {
        if((*it) == pEvent->EventID)
        {

            for(map<HI_SUBSCRIBER_S*,queue<HI_EVENT_S*>>::iterator it = sub_pre_events.begin(); it != sub_pre_events.end(); it++)
            {
                //printf("push \n");
                pthread_mutex_lock(&_mutexQueue);
                it->second.push(pEvent);
                pthread_mutex_unlock(&_mutexQueue);
                //printf("push out\n");
            }

            pthread_mutex_lock(&_historyMutex);
            HI_EVENT_S * h_event = (HI_EVENT_S *) malloc(sizeof(HI_EVENT_S));
            memcpy(h_event,pEvent,sizeof(HI_SUBSCRIBER_S));
            if(event_queue.size() < HI_EVTHUB_MESSAGEQURUR_MAX_SIZE)
            {
                event_queue.push_back(h_event);
                pthread_mutex_unlock(&_historyMutex);

                return 0;
            }else{
                free(event_queue.front());
                event_queue.pop_front();//will destroy ref,no need to free again
                //free(th->event_queue.front());
                event_queue.push_back(h_event);
                pthread_mutex_unlock(&_historyMutex);
                return 0;
            }
        }
    }
    printf("unregistered id, can not publish\n");
    return -100;
}

int EventHub::HZ_EVTHUB_GetEventHistory(HI_EVENT_ID EventID, HI_EVENT_S *pEvent)
{
    if(pEvent == NULL || pEvent == nullptr)
    {
        printf("Pointer null Error !\n");
        return -100;
    }
    memset(pEvent,0,sizeof(HI_EVENT_S));
    pthread_mutex_lock(&_historyMutex);
    list<HI_EVENT_S*>::iterator it;
    for (it = event_queue.begin(); it != event_queue.end(); it++) {
        if(EventID == (*it)->EventID)
        {
            memcpy(pEvent,*it,sizeof(HI_EVENT_S));
            pthread_mutex_unlock(&_historyMutex);
            printf("Find event histroy! %d \n",(*it)->EventID);
            return 0;
        }
    }

    pthread_mutex_unlock(&_historyMutex);
    printf("Find nothing!\n");
    return 0;
}

int EventHub::HZ_EVTHUB_SetEnabled(HZ_BOOL bFlag)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    if (pthread_mutex_lock(&_mutexConn) != 0){
        fprintf(stdout, "lock error!\n");
        return -100;
    }
    Enabled = bFlag;
    pthread_mutex_unlock(&_mutexConn);
    return 0;
}

int EventHub::HZ_EVTHUB_GetEnabled(int *pFlag)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    *pFlag = Enabled;
    return 0;
}
