#include <iostream>
#include "eventhub.h"
#include <unistd.h>
#include <string.h>
using namespace std;
static int proc_times = 1;

static int SUB_1_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{
    printf("sub1 will process event id: %d  , proc times: %d\n",pstEvent->EventID,proc_times);
    proc_times++;
    return 0;
}

static int SUB_2_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{
    printf("sub2 will process event id: %d  , proc times: %d\n",pstEvent->EventID,proc_times);
    proc_times++;
    return 0;
}

static int SUB_3_TEST_EventProc(HI_EVENT_S* pstEvent,void* pvArg)
{
    printf("sub3 will process event id: %d  , proc times: %d\n",pstEvent->EventID,proc_times);
    proc_times++;
    return 0;
}

static void Test_Sub_Pub(EventHub *hub)
{
    if(hub == NULL)
        return;

    hub->EVTHUB_Init();

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

    sleep(15);
    hub->HZ_EVTHUB_UnSubscribe(pvSubscriberID2,12354863);
    printf("will publish sth...\n");
    HI_EVENT_S event;
    char* load = "Hello world";
    event.EventID = 12354863;
    event.arg1 = 50;
    event.arg2 = 100;
    event.s32Result = 80;
    event.u64CreateTime = 99;
    memset(event.aszPayload,0,EVENT_PAYLOAD_LEN);
    strcpy(event.aszPayload,load);

    hub->HZ_EVTHUB_Register(event.EventID);

    for(int i = 1; i <= 1000 ; i++)
    {
        hub->HZ_EVTHUB_Publish(&event);
        printf("push %d times\n",i);
    }
}

int main()
{
    cout << "Hello World!" << endl;
    EventHub hub;
    Test_Sub_Pub(&hub);

    sleep(5);
    //hub.EVTHUB_Deinit();
    while (1) {
        sleep(1);
    }
    return 0;
}
