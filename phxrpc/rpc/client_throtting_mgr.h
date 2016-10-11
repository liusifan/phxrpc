
#pragma once

#include <map>
#include <string.h>
#include <time.h>

#include "phxrpc/qos.h"

namespace phxrpc {

#define REQUEST_INFO_CNT 120

typedef struct _RequestInfo {
    int request_cnt;
    int accept_cnt;
} RequestInfo;

typedef struct _ThrottingInfo {
    time_t timestamp;
    RequestInfo request_info[REQUEST_INFO_CNT];
    int total_request_cnt;
    int total_accept_cnt;

    _ThrottingInfo()
    {
        Reset();
    }

    void Reset()
    {
        memset(this, 0x0, sizeof(_ThrottingInfo));
        timestamp = time(NULL);
    }

} ThrottingInfo;


typedef map<FRRouteIPInfo, ThrottingInfo *, FRRouteIPInfoLess> ThrottingInfoMap;

class ClientThrottingMgr
{
public:
    static ClientThrottingMgr * GetDefault();

    ClientThrottingMgr();
    ~ClientThrottingMgr();

    void Report(const char * ip, const unsigned int port, int ret_code);
    void Report(const unsigned int ip, const unsigned int port, int ret_code);
    bool IsReject(const char * ip, const unsigned int port);

private:

    ThrottingInfoMap throtting_info_map_;
    int para_k_;

};

}
