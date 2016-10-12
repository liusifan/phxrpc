
#include <random>
#include <chrono>

#include "phxrpc/file.h"
#include "phxrpc/network.h"
#include "client_throtting_mgr.h"

namespace phxrpc {

ClientThrottingMgr * ClientThrottingMgr::GetDefault()
{
    static __thread ClientThrottingMgr * mgr = new ClientThrottingMgr();
    return mgr;
}

ClientThrottingMgr::ClientThrottingMgr()
{
    para_k_ = 2; //FIXME hardcode
}

ClientThrottingMgr::~ClientThrottingMgr()
{
    for(auto & it : throtting_info_map_) {
        delete it.second;
        it.second = nullptr;
    }
    throtting_info_map_.clear();
}


void ClientThrottingMgr::Report(const char * ip, const unsigned int port, int ret_code)
{
    struct in_addr addr;
    if(0 < inet_pton(AF_INET, ip, &addr)) {
        return Report(addr.s_addr, port, ret_code);
    } else {
        log(LOG_ERR, "ClientThrottingMgr::%s ip %s port %u inet_pton failed errno %d ",
                __func__, ip, port, errno);
    }

}

void ClientThrottingMgr::Report(const unsigned int ip, const unsigned int port, int ret_code)
{
    FRRouteIPInfo route_ip_info = {ip, port};

    auto it = throtting_info_map_.find(route_ip_info);
    ThrottingInfo * throtting_info = nullptr;
    if(it == throtting_info_map_.end()) {
        throtting_info = new ThrottingInfo;
        throtting_info_map_[route_ip_info] = throtting_info;
    } else {
        throtting_info = it->second;
    }

    time_t curr_time = time(nullptr);
    if((curr_time - throtting_info->timestamp)  >= REQUEST_INFO_CNT) {
        throtting_info->Reset();
    }

    int idx = curr_time % REQUEST_INFO_CNT;
    if(curr_time != throtting_info->timestamp) {
        int last_idx =  (throtting_info->timestamp + 1) % REQUEST_INFO_CNT;

        while(last_idx != idx) {
            throtting_info->total_request_cnt -= throtting_info->request_info[last_idx].request_cnt;
            throtting_info->total_accept_cnt -= throtting_info->request_info[last_idx].accept_cnt;
            throtting_info->request_info[last_idx].request_cnt = 0;
            throtting_info->request_info[last_idx].accept_cnt = 0;

            last_idx = (last_idx + 1) % REQUEST_INFO_CNT;
        }

        throtting_info->total_request_cnt -= throtting_info->request_info[last_idx].request_cnt;
        throtting_info->total_accept_cnt -= throtting_info->request_info[last_idx].accept_cnt;
        throtting_info->request_info[last_idx].request_cnt = 0;
        throtting_info->request_info[last_idx].accept_cnt = 0;

        throtting_info->timestamp = curr_time;
    }

    throtting_info->request_info[idx].request_cnt += 1;
    throtting_info->total_request_cnt += 1;

    if(phxrpc::SocketStreamError_Connnect_Error != ret_code && 
            phxrpc::SocketStreamError_Timeout != ret_code && 
            phxrpc::SocketStreamError_FastReject != ret_code && 
            phxrpc::SocketStreamError_Send_Error != ret_code) {
        throtting_info->request_info[idx].accept_cnt += 1;
        throtting_info->total_accept_cnt += 1;
    }

}

bool ClientThrottingMgr::IsReject(const char * ip, const unsigned int port)
{
    struct in_addr addr;
    if(0 >= inet_pton(AF_INET, ip, &addr)) {
        log(LOG_ERR, "ClientThrottingMgr::%s ip %s port %u inet_pton failed errno %d ",
                __func__, ip, port, errno);
        return false;
    }

    FRRouteIPInfo route_ip_info = {addr.s_addr, port};

    auto it = throtting_info_map_.find(route_ip_info);
    ThrottingInfo * throtting_info = nullptr;
    if(it == throtting_info_map_.end()) {
        return false;
    } else {
        throtting_info = it->second;
    }

    time_t curr_time = time(nullptr);
    if((curr_time - throtting_info->timestamp)  >= REQUEST_INFO_CNT) {
        throtting_info->Reset();
        log(LOG_ERR, "ClientThrottingMgr::%s ip %s port %u curr_time %u timestamp %u",
                __func__, ip, port, (unsigned int)curr_time,
                (unsigned int)throtting_info->timestamp);
        return false;
    }

    //log(LOG_INFO, "ClientThrottingMgr::%s ip %s port %u total_request_cnt %d total_accept_cnt %d",
            //__func__, ip, port, throtting_info->total_request_cnt,
            //throtting_info->total_accept_cnt);

    if(throtting_info->total_request_cnt <= para_k_ * throtting_info->total_accept_cnt) {
        return false;
    }

    int seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine eng(seed);    
    std::uniform_real_distribution<double> dis(0.0, 1.0); 

    double pro = (double)(throtting_info->total_request_cnt - para_k_ * throtting_info->total_accept_cnt) / (double)(throtting_info->total_request_cnt + 1);
    pro = std::max(pro, 0.0);

    double cli_pro = dis(eng);

    //log(LOG_INFO, "ClientThrottingMgr::%s pro %f cli_pro %f", __func__, pro, cli_pro);
    if(cli_pro < pro) {                                                                                                 
        return true;
    }                                                                                                                   
    return false;
}


}

