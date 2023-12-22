#include "eventhub.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

static map<HI_EVENT_ID, vector<HI_SUBSCRIBER_S*>> sMap;   // 消息订阅map,一个订阅者可订阅多个消息

pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

EventHub::EventHub()
{
    Enabled = HZ_TRUE;
    initFlag = HZ_FALSE;
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
    initFlag = HZ_TRUE;
    pthread_mutex_init(&_mutexSubscriber, nullptr);
    pthread_mutex_init(&_mutexPublish, nullptr);
    pthread_mutex_init(&_mutexConn, nullptr);

    pthread_mutex_lock(&_mutexConn);
    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (NULL == conn) {
        return -100;
    }

    int s32Ret = dbus_bus_request_name(conn, "test.method.server",
                                       DBUS_NAME_FLAG_REPLACE_EXISTING
                                       , &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != s32Ret) {
        return -200;
    }

    dbus_bus_add_match(conn, "type='signal',interface='aa.bb.cc'",  &err);
    dbus_connection_flush(conn);
    if(dbus_error_is_set(&err))
    {
        printf("add Match Error %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return -400;
    }


    s32Ret = pthread_create(&pt_id,NULL,EventProcess,this);
    if(s32Ret != 0){
        printf ("Create listen pthread error!\n");
        return s32Ret;
    }


    pthread_mutex_unlock(&_mutexConn);

    return s32Ret;
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
    int var =1;
    for (std::map<string,HI_SUBSCRIBER_S*>::iterator it=slist.begin(); it!=slist.end(); it++)
    {
        HI_SUBSCRIBER_S* tmp = it->second;
        free(tmp);
        tmp = NULL;
    }
    //printf("sMap :%s\n",slist.at(0)->azName);
    slist.clear();
    sMap.clear();
    printf("has deinited\n");

    pthread_join(pt_id,NULL);
    pthread_mutex_unlock(&_mutexConn);
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

    map<HI_EVENT_ID, vector<HI_SUBSCRIBER_S*>>::iterator it;
    it = sMap.find(EventID);
    if (it == sMap.end())
    {
        sMap[EventID] = vector<HI_SUBSCRIBER_S*>();//若不存在则创建,存在就不做处理
    }else{
        printf("id has existed ,pointer: %ld\n",sMap[EventID].at(0)->HI_EVTHUB_EVENTPROC_FN_PTR);
    }

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
    map<HI_EVENT_ID, vector<HI_SUBSCRIBER_S*>>::iterator it;
    it = sMap.find(EventID);
    if (it != sMap.end())
        sMap.erase (it);

    pthread_mutex_unlock(&_mutexPublish);
    return 0;
}

int EventHub::HZ_EVTHUB_Publish(HI_EVENT_S *pEvent)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    if (pthread_mutex_lock(&_mutexPublish) != 0){
        fprintf(stdout, "lock error!\n");
    }
    map<HI_EVENT_ID, vector<HI_SUBSCRIBER_S*>>::iterator it;
    it = sMap.find(pEvent->EventID);
    if (it == sMap.end())
    {
        printf("error :event has not registered\n");
        pthread_mutex_unlock(&_mutexPublish);
        return -100;
    }else{
        printf("id has registered ,pointer: %ld\n",sMap[pEvent->EventID].at(0)->HI_EVTHUB_EVENTPROC_FN_PTR);
    }


    dbus_uint32_t serial = 0; // unique number to associate replies with requests
    DBusMessage* msg;
    char *buff = pEvent->aszPayload;

    // create a signal and check for errors
    msg = dbus_message_new_signal("/mypusher", // object name of the signal
                                  "aa.bb.cc", // interface name of the signal
                                  "test"); // name of the signal
    if (NULL == msg)
    {
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
    if (!dbus_connection_send(conn, msg, &serial)) {
        printf("Out Of Memory!\n");
        return -400;
    }
    dbus_connection_flush(conn);

    // free the message
    dbus_message_unref(msg);

    pthread_mutex_unlock(&_mutexPublish);
    return 0;
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
        printf("Please create a subcriber first!\n");
        pthread_mutex_unlock(&_mutexSubscriber);
        return -200;
    }
    //没有注册的ID能订阅么?
    HI_SUBSCRIBER_S*sub = (HI_SUBSCRIBER_S *)pSubscriber;
    map<string,HI_SUBSCRIBER_S*>::iterator it;
    it = slist.find(sub->azName);
    int i=0;
    if(it != slist.end())
    {
        sMap[EventID].push_back(sub);
        //printf("push sMap id : %d   ---  pointer: %ld\n",EventID,sMap[EventID][i]->HI_EVTHUB_EVENTPROC_FN_PTR);
        pthread_mutex_unlock(&_mutexSubscriber);

        return 0;
    }else{
        printf("subcriber has not create yet! \n");
        pthread_mutex_unlock(&_mutexSubscriber);
        return -400;
    }
    pthread_mutex_unlock(&_mutexSubscriber);
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
    map<string,HI_SUBSCRIBER_S*>::iterator it;
    vector<HI_SUBSCRIBER_S*>::iterator v_it;
    it = slist.find(sub->azName);
    if(it != slist.end())
    {
        //sMap[EventID].remove((it->second));
        v_it = find(sMap[EventID].begin(),sMap[EventID].end(),sub);
        if(v_it != sMap[EventID].end())
        {
            sMap[EventID].erase(v_it);
        }
        pthread_mutex_unlock(&_mutexSubscriber);

        return 0;
    }else{
        printf("subcriber has not create yet! \n");
        pthread_mutex_unlock(&_mutexSubscriber);
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
    pthread_mutex_lock(&_mutexSubscriber);

    HI_SUBSCRIBER_S *sub_c = (HI_SUBSCRIBER_S *)malloc(sizeof(HI_SUBSCRIBER_S));
    memset(sub_c,0,sizeof(HI_SUBSCRIBER_S));
    memcpy(sub_c,pstSubscriber,sizeof(HI_SUBSCRIBER_S));
    slist[pstSubscriber->azName]  = sub_c;
    *ppSubscriber = sub_c;
    printf("creat a new subcriber, func pointer: %ld \n",slist[pstSubscriber->azName]->HI_EVTHUB_EVENTPROC_FN_PTR);
    pthread_mutex_unlock(&_mutexSubscriber);

    return 0;
}

int EventHub::HZ_EVTHUB_DestroySubscriber(HI_SUBSCRIBER_S *pstSubscriber)
{
    if(initFlag == HZ_FALSE)
    {
        printf("has not inited\n");
        return -1;
    }
    pthread_mutex_lock(&_mutexSubscriber);
    //移除所有事件的订阅者列表中的该订阅者
    map<HI_EVENT_ID, vector<HI_SUBSCRIBER_S*>>::iterator it;
    vector<HI_SUBSCRIBER_S*>::iterator v_it;
    for (it=sMap.begin(); it!=sMap.end(); ++it)
    {
        for (int var = 0; var < it->second.size(); var++) {
            if(strcmp(pstSubscriber->azName ,it->second[var]->azName) == 0)
            {
                it->second.erase(it->second.begin() + var);
            }
        }

    }
    free(slist[pstSubscriber->azName]);
    slist.erase(pstSubscriber->azName);
    printf("remove a subcriber\n");
    pthread_mutex_unlock(&_mutexSubscriber);
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

void *EventHub::EventProcess(void *_this)
{  
    EventHub * th = (EventHub *)_this;
    DBusMessage *msg;
    DBusMessageIter arg;

    int count = 1;
    printf("handle thread: %5u\n", gettid());
    while(1)
    {
        if(th->Enabled == HZ_FALSE || th->initFlag == HZ_FALSE)
        {
            usleep(1000*10);
            continue;
        }

        //param1: 连接描述符
        //param2: 超时时间，　-1无限超时时间
        dbus_connection_read_write(th->conn, 0);
        //从队列中取出一条消息
        msg = dbus_connection_pop_message(th->conn);
        if(msg == NULL)
        {
            usleep(1000*100);
            continue;
        }

        if(dbus_message_is_signal(msg, "aa.bb.cc", "test"))
        {
            //printf("recv event count: %d\n",count);

            dbus_message_iter_init (msg, &arg);
            int current_type;
            HI_EVENT_S event = {0,0,0,0,0,0};
            while ((current_type = dbus_message_iter_get_arg_type (&arg)) != DBUS_TYPE_INVALID)
            {
                if(current_type == DBUS_TYPE_STRING)
                {
                    char *param;
                    dbus_message_iter_get_basic(&arg,&param);
                    strcpy(event.aszPayload,param);
                    //printf("recv no.%d param --: %s\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT32)
                {
                    HI_EVENT_ID event_id = 0;
                    dbus_message_iter_get_basic(&arg,&event_id);
                    event.EventID = event_id;
                    printf("recv event id: %d\n", event.EventID);
                }else if(current_type == DBUS_TYPE_INT32)
                {
                    int param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    if(count == 2)
                        event.arg1 = param;
                    else if(count == 3)
                        event.arg2 = param;
                    else if(count == 4)
                        event.s32Result = param;
                    //printf("recv no.%d param --: %d\n", count,param);
                }else if(current_type == DBUS_TYPE_UINT64)
                {
                    unsigned long param = 0;
                    dbus_message_iter_get_basic(&arg,&param);
                    event.u64CreateTime = param;
                    //printf("recv no.%d param --: %ld\n", count,param);
                }
                dbus_message_iter_next (&arg);
                count++;
            }
            count = 1;
            //推送消息给订阅者
            printf("start send event to subcribers ...\n");
            map<HI_EVENT_ID,vector<HI_SUBSCRIBER_S*>>::const_iterator it;
            it = sMap.find(event.EventID);
            if (it != sMap.end())
            {

                vector<HI_SUBSCRIBER_S*> sub_list = it->second;
                if(sub_list.empty())
                {
                    printf("can't find any subcriber\n");
                }

                for (int i =0 ; i < sub_list.size() ; i++)
                {
                    sub_list.at(i)->HI_EVTHUB_EVENTPROC_FN_PTR(&event,NULL);
                }
            }else{
                printf("can't find this Event,please register it first\n");
            }
        }
        //释放空间
        dbus_message_unref(msg);

    }
    return nullptr;
}
