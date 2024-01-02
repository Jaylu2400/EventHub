#include "eventhub.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <sched.h>
#include<iostream>
#include <thread>
#include <future>

DBusError client_err;
DBusConnection *client_conn;
map<HI_SUBSCRIBER_S*, queue<HI_EVENT_S*>> e_map_queue;

pthread_mutex_t _mutexQueue; // 事件历史队列互斥锁
pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t _mutexMsg = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _condQueue ,_condMsg; //条件变量
typedef struct hzThreadParams {
    void *_this; //类的this指针
    HI_SUBSCRIBER_S * sub;//订阅者指针

} HZ_HANDLE_THREAD_PARAMS;

void mycleanfunc(void *arg) //清理函数
{
    if(arg != NULL)
    {
        dbus_connection_close((DBusConnection*)arg);
        dbus_connection_unref((DBusConnection*)arg);
    }
    //pthread_mutex_unlock(&_mutexQueue);//清理时，解锁以避免死锁
    printf("mycleanfunc\n");
}
pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

EventHub::EventHub()
    :pool(10)
{
    Enabled = HZ_TRUE;
    initFlag = HZ_FALSE;
    memset(&event,0,sizeof(HI_EVENT_S));

}

EventHub::~EventHub()
{
    if(initFlag == HZ_TRUE)
        EVTHUB_Deinit();
}

int EventHub::EVTHUB_Init()
{
    if(initFlag == HZ_TRUE)
    {
        printf("has inited,no need to init\n");
        return 0;
    }

    pthread_mutex_init(&_mutexSubscriber, nullptr);
    pthread_mutex_init(&_mutexPublish, nullptr);
    pthread_mutex_init(&_mutexConn, nullptr);
    pthread_mutex_init(&_mutexQueue, nullptr);
    //pthread_mutex_init(&_mutexMsg, nullptr);


    pthread_mutex_lock(&_mutexConn);
    initFlag = HZ_TRUE;
    _condQueue = PTHREAD_COND_INITIALIZER;
    _condMsg = PTHREAD_COND_INITIALIZER;

    client_conn = dbus_bus_get(DBUS_BUS_SESSION, &client_err);

    if (dbus_error_is_set(&client_err)) {
        printf("Connection Error (%s)\n", client_err.message);
        dbus_error_free(&client_err);
    }
    if (NULL == client_conn) {
        printf("Conn Error (%s)\n", client_err.message);
        //exit(-100);
    }


    int s32Ret = dbus_bus_request_name(client_conn, "test.client.aa",
                                       DBUS_NAME_FLAG_REPLACE_EXISTING
                                       , &client_err);
    if (dbus_error_is_set(&client_err)) {
        printf("Name Error (%s)\n", client_err.message);
        dbus_error_free(&client_err);
        //return NULL;
    }

    pool.init();
    s32Ret = pthread_create(&event_history_loop,NULL,EventHistoryHandle,this);
    if(s32Ret != 0){
        pthread_mutex_unlock(&_mutexConn);
        printf ("Create listen pthread error!\n");
        return s32Ret;
    }
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
    pool.shutdown();
    Enabled = HZ_FALSE;
    pthread_cancel(event_history_loop);
    pthread_join(event_history_loop,NULL);


    while (!event_queue.empty())
    {
        list<HI_EVENT_S*>::iterator it1;
        it1 = event_queue.end();
        HI_EVENT_S *p = *it1;
        if(p != NULL)
        {
            printf("free mem\n");
            free(p);
            p = NULL;
            event_queue.pop_back();
        }

    }




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

int EventHub::HZ_EVTHUB_Publish(HI_EVENT_S *pEvent)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }


    vector<HI_EVENT_ID>::iterator it;
    for(it = plist.begin(); it != plist.end() ; it++)
    {
        if((*it) == pEvent->EventID)
        {
            if (pthread_mutex_lock(&_mutexPublish) != 0){
                fprintf(stdout, "lock error!\n");
            }

            dbus_uint32_t serial = 0; // unique number to associate replies with requests
            DBusMessage* msg;
            char *buff = pEvent->aszPayload;
            char name[32];
            sprintf(name,"%u",pEvent->EventID);
            string pre("aa.bb.");
            pre += string(name);

            // create a signal and check for errors
            msg = dbus_message_new_signal("/mypusher", // object name of the signal
                                          "aa.bb.cc", // interface name of the signal
                                          "test"); // name of the signal
            if (NULL == msg)
            {
                pthread_mutex_unlock(&_mutexPublish);
                printf("Message Null\n");
                return -600;
            }

            dbus_message_append_args(msg,DBUS_TYPE_UINT32, &pEvent->EventID,
                                     DBUS_TYPE_INT32, &pEvent->arg1,
                                     DBUS_TYPE_INT32, &pEvent->arg2,
                                     DBUS_TYPE_INT32, &pEvent->s32Result,
                                     DBUS_TYPE_UINT64,&pEvent->u64CreateTime,
                                     DBUS_TYPE_STRING,&buff,
                                     DBUS_TYPE_INVALID);

            // send the message and flush the connection
            if (!dbus_connection_send(client_conn, msg, &serial)) {
                printf("Out Of Memory!\n");
                pthread_mutex_unlock(&_mutexPublish);
                return -400;
            }

            dbus_connection_flush(client_conn);

            // free the message
            dbus_message_unref(msg);
            //printf("unref msg\n");

            pthread_mutex_unlock(&_mutexPublish);
            return 0;
        }
    }


    printf("unregistered id, can not publish\n");
    return -100;
}

int EventHub::HZ_EVTHUB_Subscribe(HI_MW_PTR pSubscriber, HI_EVENT_ID EventID)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
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
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    if(pSubscriber == NULL)
    {
        printf("subcriber has not created,please create a subcriber first!\n");
        return -200;
    }
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

int EventHub::HZ_EVTHUB_CreateSubscriber(HI_SUBSCRIBER_S *pstSubscriber, HI_MW_PTR *ppSubscriber)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    if(*ppSubscriber != NULL)
    {
        printf("create subcriber error!\n");
        return -200;
    }
    if(sub_threads.size() > HZ_EVTHUB_MAX_SUBSCRIBERS)
    {
        printf("Can't create a subscriber any more!\n");
        return -500;
    }


    pthread_mutex_lock(&_mutexSubscriber);

    HI_SUBSCRIBER_S *sub_c = (HI_SUBSCRIBER_S *)malloc(sizeof(HI_SUBSCRIBER_S));
    memset(sub_c,0,sizeof(HI_SUBSCRIBER_S));
    memcpy(sub_c,pstSubscriber,sizeof(HI_SUBSCRIBER_S));
    //    sub_c->bSync = HZ_TRUE;
    //    slist[pstSubscriber->azName]  = sub_c;
    *ppSubscriber = sub_c;
    //printf("will create a suber\n");
    list<HI_EVENT_ID> e_list;
    sub_event_list.insert(pair<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>(sub_c,e_list));

    pthread_t sub_t;
    HZ_HANDLE_THREAD_PARAMS *p = (HZ_HANDLE_THREAD_PARAMS *)malloc(sizeof(HZ_HANDLE_THREAD_PARAMS ));
    p->sub = sub_c;
    p->_this = this;
    if(pthread_create(&sub_t,NULL,EventProcess,(void*)p) != 0)
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

    pthread_mutex_unlock(&_mutexSubscriber);
    printf("remove a subcriber\n");
    return 0;
}

int EventHub::HZ_EVTHUB_GetEventHistory(HI_EVENT_ID EventID, HI_EVENT_S *pEvent)
{
    if(pEvent == NULL || pEvent == nullptr)
    {
        printf("Pointer null Error !\n");
        return -100;
    }
    memset(pEvent,0,sizeof(HI_EVENT_S));

    pthread_mutex_lock(&_mutexQueue);
    list<HI_EVENT_S*>::iterator it;
    for (it = event_queue.begin(); it != event_queue.end(); it++) {
        if(EventID == (*it)->EventID)
        {
            memcpy(pEvent,*it,sizeof(HI_EVENT_S));
            pthread_mutex_unlock(&_mutexQueue);
            printf("Find event histroy! %d \n",(*it)->EventID);
            return 0;
        }
    }

    pthread_mutex_unlock(&_mutexQueue);
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
    Enabled = bFlag;
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

void *EventHub::EventProcess(void *args)
{  
    HZ_HANDLE_THREAD_PARAMS *p = (HZ_HANDLE_THREAD_PARAMS *)args;
    HI_SUBSCRIBER_S * sub = p->sub;
    EventHub * th = (EventHub *)p->_this;

    DBusConnection *conn;
    DBusError err;


    conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);//多线程每个线程需要建立专用连接
    dbus_threads_init_default();

    if (dbus_error_is_set(&err)) {
        printf("Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (NULL == conn) {
        printf("Conn Error (%s)\n", err.message);
        //exit(-100);
    }
    string pre("test.server.");
    pre += string(sub->azName);
    //cout << "conn name: "<< pre << endl;
    const char* name = pre.c_str();

    int s32Ret = dbus_bus_request_name(conn, name,
                                       DBUS_NAME_FLAG_REPLACE_EXISTING
                                       , &err);
    if (dbus_error_is_set(&err)) {
        printf("Name Error (%s)\n", err.message);
        dbus_error_free(&err);
        //return NULL;
    }

    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != s32Ret) {
        //printf(" Error (%s)\n", err.message);
        //exit(-200);
    }
    //printf("1111111111111111111   %d\n",s32Ret);
    dbus_bus_add_match(conn, "type='signal',interface='aa.bb.cc'",  &err);
    dbus_connection_flush(conn);
    if(dbus_error_is_set(&err))
    {
        printf("add Match Error %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        //exit(-400);
    }

    //param1: 连接描述符
    //param2: 超时时间，　-1无限超时时间
    //dbus_connection_read_write_dispatch(conn, 0);


    pthread_cleanup_push(free, args);
    pthread_cleanup_push(mycleanfunc,conn);
    //pthread_cleanup_push(free, conn);

    int count = 1;
    printf("main loop thread: %5u,  priority: %d\n", gettid(),getpid());

    while(1)
    {
        if(th->Enabled == HZ_FALSE || th->initFlag == HZ_FALSE)
        {
            usleep(1000*10);
            continue;
        }

        DBusMessage *msg;
        DBusMessageIter arg;

        dbus_connection_read_write(conn,0);

        msg = dbus_connection_pop_message(conn);
        if(msg == NULL)
        {
            //printf("msg is NULL\n");
            usleep(1000*10);
            continue;
        }
//        DBusDispatchStatus status = dbus_connection_get_dispatch_status(conn);
//        if( status == DBUS_DISPATCH_DATA_REMAINS)
//        {
//            printf("DBUS_DISPATCH_DATA_REMAINS \n");
//        }else if(status ==DBUS_DISPATCH_NEED_MEMORY)
//        {
//            printf("DBUS_DISPATCH_COMPLETE  \n");
//        }else{
//            printf("DBUS_DISPATCH_NEED_MEMORY \n");
//        }

        if(dbus_message_is_signal(msg, "aa.bb.cc", "test"))
        {
            //printf("recv event count: %d\n",count);
            //printf("handle thread: %5u\n", gettid());
            dbus_message_iter_init (msg, &arg);
            int current_type;
            //HI_EVENT_S *event = (HI_EVENT_S*)malloc(sizeof(HI_EVENT_S));
            memset(&th->event,0,sizeof(HI_EVENT_S));
            while ((current_type = dbus_message_iter_get_arg_type (&arg)) != DBUS_TYPE_INVALID)
            {
                if(current_type == DBUS_TYPE_STRING)
                {
                    char *param;
                    dbus_message_iter_get_basic(&arg,&param);
                    strcpy(th->event.aszPayload,param);
                    //                    printf("recv no.%d param --: %s\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT32)
                {
                    HI_EVENT_ID event_id = 0;
                    dbus_message_iter_get_basic(&arg,&event_id);
                    th->event.EventID = event_id;
                    //                    char id[32] = {0};
                    //                    sprintf(id,"%u",event_id);
                    //                    printf("recv event id: %s   thread: %5u\n", id,gettid());
                }else if(current_type == DBUS_TYPE_INT32)
                {
                    int param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    if(count == 2)
                        th->event.arg1 = param;
                    else if(count == 3)
                        th->event.arg2 = param;
                    else if(count == 4)
                        th->event.s32Result = param;
                    //printf("recv no.%d param --: %d\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT64)
                {
                    unsigned long param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    th->event.u64CreateTime = param;
                    //printf("recv no.%d param --: %ld\n", count,param);
                }
                dbus_message_iter_next (&arg);
                count++;
            }

            map<HI_SUBSCRIBER_S*,list<HI_EVENT_ID>>::iterator it;

            for (list<HI_EVENT_ID>::iterator e_it = th->sub_event_list[sub].begin(); e_it != th->sub_event_list[sub].end(); e_it++) {
                if((*e_it) == th->event.EventID)
                {
                    sub->HI_EVTHUB_EVENTPROC_FN_PTR(&th->event,NULL);
                }
            }


        }else {
            //printf("not a msg\n");
            //usleep(10*1000);
        }
        //break;
        //usleep(100*1000);
        dbus_message_unref(msg);
        pthread_testcancel();
    }

    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    printf("this line will not run\n");
    return NULL;
}

void *EventHub::EventHistoryHandle(void *p)
{
    EventHub * th = (EventHub *)p;
    dbus_bus_add_match(client_conn, "type='signal',interface='aa.bb.cc'",  &client_err);
    dbus_connection_flush(client_conn);
    if(dbus_error_is_set(&client_err))
    {
        printf("add Match Error %s--%s\n", client_err.name, client_err.message);
        dbus_error_free(&client_err);
        //exit(-400);
    }

    printf("history thread: %5u,  priority: %d\n", gettid(),getpid());
    int count = 1;
    while (1) {
        if(th->Enabled == HZ_FALSE || th->initFlag == HZ_FALSE)
        {
            usleep(1000*10);
            continue;
        }

        DBusMessage *msg;
        DBusMessageIter arg;

        dbus_connection_read_write(client_conn,0);

        msg = dbus_connection_pop_message(client_conn);
        if(msg == NULL)
        {
            usleep(1000*10);
            continue;
        }

        if(dbus_message_is_signal(msg, "aa.bb.cc", "test"))
        {
            //printf("recv event count: %d\n",count);
            //printf("handle thread: %5u\n", gettid());
            dbus_message_iter_init (msg, &arg);
            int current_type;
            HI_EVENT_S *event = (HI_EVENT_S*)malloc(sizeof(HI_EVENT_S));
            memset(event,0,sizeof(HI_EVENT_S));
            while ((current_type = dbus_message_iter_get_arg_type (&arg)) != DBUS_TYPE_INVALID)
            {
                if(current_type == DBUS_TYPE_STRING)
                {
                    char *param;
                    dbus_message_iter_get_basic(&arg,&param);
                    strcpy(event->aszPayload,param);
                    //                    printf("recv no.%d param --: %s\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT32)
                {
                    HI_EVENT_ID event_id = 0;
                    dbus_message_iter_get_basic(&arg,&event_id);
                    event->EventID = event_id;
                    //                    char id[32] = {0};
                    //                    sprintf(id,"%u",event_id);
                    //                    printf("recv event id: %s   thread: %5u\n", id,gettid());
                }else if(current_type == DBUS_TYPE_INT32)
                {
                    int param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    if(count == 2)
                        event->arg1 = param;
                    else if(count == 3)
                        event->arg2 = param;
                    else if(count == 4)
                        event->s32Result = param;
                    //printf("recv no.%d param --: %d\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT64)
                {
                    unsigned long param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    event->u64CreateTime = param;
                    //printf("recv no.%d param --: %ld\n", count,param);
                }

                pthread_mutex_lock(&_mutexQueue);
                if(th->event_queue.size() < HI_EVTHUB_MESSAGEQURUR_MAX_SIZE)
                {
                    th->event_queue.push_back(event);
                    pthread_mutex_unlock(&_mutexQueue);
                }else{
                    th->event_queue.pop_front();//will destroy ref,no need to free again
                    //free(th->event_queue.front());
                    th->event_queue.push_back(event);
                    pthread_mutex_unlock(&_mutexQueue);
                }

                dbus_message_iter_next (&arg);
            }

        }



        dbus_message_unref(msg);
        pthread_testcancel();
    }
}
