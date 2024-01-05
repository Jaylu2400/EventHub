#include <iostream>
#include <unistd.h>
#include "eventhub.h"
#include <malloc.h>
using namespace std;
static int proc_times1 = 0,proc_times2 = 0,proc_times3 = 0;
pthread_mutex_t mutex1= PTHREAD_MUTEX_INITIALIZER ,mutex2= PTHREAD_MUTEX_INITIALIZER,mutex3= PTHREAD_MUTEX_INITIALIZER;

static int SUB_1_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{

    //sleep(5);
    pthread_mutex_lock(&mutex1);
    proc_times1++;
    printf("sub1 will process event id: %d  , proc times: %d \n",pstEvent->EventID,proc_times1);

    pthread_mutex_unlock(&mutex1);

    return 0;
}

static int SUB_2_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{
    //sleep(1);
    pthread_mutex_lock(&mutex2);
    proc_times2++;
    printf("sub2 will process event id: %d  , proc times: %d \n",pstEvent->EventID,proc_times2);

    pthread_mutex_unlock(&mutex2);
    return 0;
}

static int SUB_3_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{

    pthread_mutex_lock(&mutex3);
    proc_times3++;
    printf("sub3 will process event id: %d  , proc times: %d \n",pstEvent->EventID,proc_times3);

    pthread_mutex_unlock(&mutex3);
    return 0;
}

static void *Test_Sub_Pub(void* arg)
{
    EventHub *hub = (EventHub*) arg;
    if(hub == NULL)
        return NULL;
    HI_MW_PTR pvSubscriberID = NULL;
    HI_SUBSCRIBER_S stSubscriber = {"SUBCRIBER_1",
                                        SUB_1_TEST_EventProc,
                                        NULL, HZ_FALSE};
    hub->HZ_EVTHUB_CreateSubscriber(&stSubscriber, &pvSubscriberID);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID,1111);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID,2222);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID,3333);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID,12354863);

    HI_MW_PTR pvSubscriberID2 = NULL;
    HI_SUBSCRIBER_S stSubscriber2 = {"SUBCRIBER_2",
                                        SUB_2_TEST_EventProc,
                                        NULL, HZ_FALSE};
    hub->HZ_EVTHUB_CreateSubscriber(&stSubscriber2, &pvSubscriberID2);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID2,8888);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID2,9999);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID2,7777);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID2,12354863);

    HI_MW_PTR pvSubscriberID3 = NULL;
    HI_SUBSCRIBER_S stSubscriber3 = {"SUBCRIBER_3",
                                        SUB_3_TEST_EventProc,
                                        NULL, HZ_FALSE};
    hub->HZ_EVTHUB_CreateSubscriber(&stSubscriber3, &pvSubscriberID3);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID3,5555);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID3,1234);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID3,6789);
    hub->HZ_EVTHUB_Subscribe(pvSubscriberID3,12354863);
    //hub.HZ_EVTHUB_UnSubscribe(pvSubscriberID3,12354863);
    printf("will SLEEP...\n");
    sleep(5);
    printf("will publish sth...\n");
    char* load = "Hello world";
    HI_EVENT_S event = {0};
    event.EventID = 12354863;
    event.arg1 = 50;
    event.arg2 = 100;
    event.s32Result = 80;
    event.u64CreateTime = 99;
    memset(event.aszPayload,0,EVENT_PAYLOAD_LEN);
    strcpy(event.aszPayload,load);

    //hub.HZ_EVTHUB_Publish(&event);
    hub->HZ_EVTHUB_Register(event.EventID);
    for(int i=0 ; i< 10000000;i++)
    {

        hub->HZ_EVTHUB_Publish(&event);
    }
    //printf("push 1 times\n");
    /*sleep(10);
    hub.EVTHUB_Deinit();
    malloc_trim(0)*/;
    return NULL;
}
int main()
{
    cout << "Hello World!" << endl;
    EventHub hub;
    hub.EVTHUB_Init();
    Test_Sub_Pub(&hub);
    //sleep(40);
    //hub.EVTHUB_Deinit();
    //malloc_trim(0);
    while (1) {
        sleep(1);
    }
    return 0;
}
