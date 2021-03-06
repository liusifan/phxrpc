/*
Tencent is pleased to support the open source community by making 
PhxRPC available.
Copyright (C) 2016 THL A29 Limited, a Tencent company. 
All rights reserved.

Licensed under the BSD 3-Clause License (the "License"); you may 
not use this file except in compliance with the License. You may 
obtain a copy of the License at

https://opensource.org/licenses/BSD-3-Clause

Unless required by applicable law or agreed to in writing, software 
distributed under the License is distributed on an "AS IS" basis, 
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
implied. See the License for the specific language governing 
permissions and limitations under the License.

See the AUTHORS file for names of contributors.
*/

#pragma once

#include <set>

#include "phxrpc/qos.h"
#include "client_config.h"
#include "client_throtting_mgr.h"

namespace phxrpc {

template <typename CallStub, typename GetEndpoint>
int ClientCall( ClientConfig & config, ClientMonitor & monitor, CallStub call_stub, GetEndpoint get_endpoint )
{
    int ret = -1;

    std::set< const Endpoint_t * > in_use_set;

    int retry_cnt = config.GetRetryCnt();
    for( int i = 0; i < retry_cnt; i++ ) {

        const phxrpc::Endpoint_t * ep = nullptr;

        for( int k = 0; k < 3; k++ ) {
            if( nullptr != ( ep = get_endpoint( config ) ) ) {
                if( in_use_set.end() != in_use_set.find( ep ) ) {
                    ep = nullptr;
                } else {
                    break;
                }
            }
        }

        if( nullptr == ep ) return -2;
        in_use_set.insert( ep );

        if(config.IsEnableClientFastReject()) {
            //cli fr
            if(FRClient::GetDefault()->IsSvrBlocked(ep->ip, ep->port, FastRejectQoSMgr::GetReqQoSInfo())) {
                phxrpc::log(LOG_DEBUG, "%s req hit cli rfr %s ", __func__, 
                        FastRejectQoSMgr::GetReqQoSInfo()?FastRejectQoSMgr::GetReqQoSInfo():"");
                monitor.ClientFastReject();
                continue;
            }
        } else if(config.IsEnableClientThrotting()) {

            if(ClientThrottingMgr::GetDefault()->IsReject(ep->ip, ep->port)) {
                phxrpc::log(LOG_INFO, "%s req reject by client throtting ", __func__); 
                monitor.ClientFastReject();

                ClientThrottingMgr::GetDefault()->Report(ep->ip, ep->port, phxrpc::SocketStreamError_FastReject);
                continue;
            }
        }

        phxrpc::BlockTcpStream socket;


        bool open_ret = phxrpc::PhxrpcTcpUtils::Open(&socket, ep->ip, ep->port,
                    config.GetConnectTimeoutMS(), NULL, 0, monitor );

        if ( ! open_ret )  {
            if(config.IsEnableClientThrotting()) {
                ClientThrottingMgr::GetDefault()->Report(ep->ip, ep->port, phxrpc::SocketStreamError_Connnect_Error);
            }
            continue;
        }

        socket.SetTimeout( config.GetSocketTimeoutMS() );

        ret = call_stub( socket );

        if(config.IsEnableClientThrotting()) {
            ClientThrottingMgr::GetDefault()->Report(ep->ip, ep->port, ret);
        }
        if( phxrpc::SocketStreamError_FastReject != ret ) {
            break;
        }
    }

    return ret;
}

}

