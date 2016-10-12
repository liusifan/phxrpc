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

#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <string.h>

#include "phxrpc/file.h"
#include "client_monitor.h"

namespace phxrpc {

typedef struct tagEndpoint {
    char ip[32];
    int port;
} Endpoint_t;

inline bool EndPointCmp( const Endpoint_t * ep1, const Endpoint_t * ep2 ) {
    int ret = strcmp( ep1->ip, ep2->ip );

    if( ret < 0 ) {
        return true;
    } else if( ret > 0 ) {
        return false;
    } else {
        return ep1->port < ep2->port;
    }
}

class ClientConfig {
 public:
    ClientConfig();

    virtual ~ClientConfig();

    bool Read(const char * config_file);
    bool Read(const std::string & content);

    const Endpoint_t * GetRandom() const;

    const Endpoint_t * GetByIndex(const size_t index) const;

    size_t GetCount() const;

    int GetConnectTimeoutMS();

    int GetSocketTimeoutMS();

    const char * GetPackageName() const;

    void SetClientMonitor( ClientMonitorPtr client_monitor );

    ClientMonitorPtr GetClientMonitor();

    bool IsEnableClientFastReject();
    int GetOssId();

    bool IsEnableClientThrotting();

    int GetRetryCnt();

 protected:
    virtual bool Parse(Config & config);

    std::vector<Endpoint_t> endpoints_;

    int connect_timeout_ms_;
    int socket_timeout_ms_;

    char package_name_[64];

    int is_enable_cli_fr_;
    int is_enable_cli_throtting_;

    ClientMonitorPtr client_monitor_;
    int oss_id_;
    int retry_cnt_;
};

}

