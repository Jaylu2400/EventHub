#ifndef EVENTHUB_H
#define EVENTHUB_H
#include <stdio.h>
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <string.h>
#include <queue>
#include <unistd.h>
#include <pthread.h>
#include <mutex>

//#include <windows.h>
using namespace std;
#define HZ_BOOL int
#define HZ_TRUE 1
#define HZ_FALSE 0
#define EVENT_PAYLOAD_LEN               (128)
#define HI_EVTHUB_SUBSCRIBE_NAME_LEN    (16)
#define HI_EVTHUB_MESSAGEQURUR_MAX_SIZE (32)
#define HZ_EVTHUB_MAX_SUBSCRIBERS (1000)
#define HZ_EVTHUB_SUBSCRIBER_MAX_MESSAGES (1000)
typedef void*       HI_MW_PTR;
typedef unsigned int HI_EVENT_ID;

typedef struct hiEVENT_S {
    HI_EVENT_ID EventID;
    int arg1;
    int arg2;
    int s32Result;
    unsigned long u64CreateTime;
    char aszPayload[EVENT_PAYLOAD_LEN];
} HI_EVENT_S;

typedef int (*HI_EVTHUB_EVENTPROC_FN_PTR)(HI_EVENT_S*,void*);
typedef struct hiSUBSCRIBER_S {
    char azName[HI_EVTHUB_SUBSCRIBE_NAME_LEN];
    int (*HI_EVTHUB_EVENTPROC_FN_PTR)(HI_EVENT_S *pEvent, void *argv);
    void *argv;
    HZ_BOOL bSync;
} HI_SUBSCRIBER_S;




class EventHub
{
public:
    EventHub();
    ~EventHub();
    //接口

    int EVTHUB_Init();
    int EVTHUB_Deinit();
    int HZ_EVTHUB_Register(HI_EVENT_ID EventID);
    int HZ_EVTHUB_UnRegister(HI_EVENT_ID EventID);
    int HZ_EVTHUB_Publish(HI_EVENT_S *pEvent);
    int HZ_EVTHUB_Subscribe(HI_MW_PTR pSubscriber, HI_EVENT_ID EventID);
    int HZ_EVTHUB_UnSubscribe(HI_MW_PTR pSubscriber, HI_EVENT_ID EventID);
    int HZ_EVTHUB_CreateSubscriber(HI_SUBSCRIBER_S *pstSubscriber, HI_MW_PTR *ppSubscriber);
    int HZ_EVTHUB_DestroySubscriber(HI_SUBSCRIBER_S *pstSubscriber);
    int HZ_EVTHUB_GetEventHistory(HI_EVENT_ID EventID, HI_EVENT_S *pEvent);
    int HZ_EVTHUB_SetEnabled(HZ_BOOL bFlag);
    int HZ_EVTHUB_GetEnabled(HZ_BOOL *pFlag);

private:
    pthread_mutex_t _mutexSubscriber;   // 消息订阅互斥锁
    pthread_mutex_t _mutexConn;   // Dbus连接互斥锁
    pthread_mutex_t _mutexPublish;   // 消息发布互斥锁
    pthread_t main_loop;
    vector<HI_EVENT_ID> plist;   // 可发布的事件ID
    map<HI_SUBSCRIBER_S*,pthread_t> sub_threads;//订阅者线程表
};

#endif // EVENTHUB_H
