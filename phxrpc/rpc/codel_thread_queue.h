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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>
#include <map>

#include <math.h>

#include "phxrpc/network.h"
#include "phxrpc/file.h"

namespace phxrpc {

typedef struct _CoDelQueueItem {
    uint64_t enqueue_time_ms;
    bool is_drop;
    void * data;

    _CoDelQueueItem(void * _data) {
        data = _data;
        enqueue_time_ms = Timer::GetSteadyClockMS();
        is_drop = false;
    }
} CoDelQueueItem;

typedef struct _DoDequeResult {
    CoDelQueueItem item;
    bool ok_to_drop;
    bool is_empty;
} DoDequeResult;

class CoDelThdQueue {
    public:
    CoDelThdQueue() : break_out_(false), size_(0) {
        drop_count_ = 0;
        is_dropping_ = 0;
        codel_first_above_time_ = 0;
        codel_drop_next_ = 0;
        codel_target_ms_ = 5; //FIXME hardcode
        codel_interval_ms_ = 100; //FIXME hardcode
    }
    ~CoDelThdQueue() { break_out(); }

    uint64_t control_law(uint64_t t) {
        return t + codel_interval_ms_  / sqrt(drop_count_);
    }

    size_t size() {
        return static_cast<size_t>(size_);
    }

    bool empty() {
        return static_cast<size_t>(size_) == 0;
    }

    void push(const CoDelQueueItem & value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        size_++;
        cv_.notify_one();
    }

    DoDequeResult dodeque(uint64_t now) {

        DoDequeResult r = {nullptr, false, true};

        if(queue_.empty()) {
            codel_first_above_time_ = 0;
        } else {
            size_--;
            r.is_empty = false;
            r.item = queue_.front();
            queue_.pop();

            uint64_t sojourn_time = now - r.item.enqueue_time_ms;
            if (sojourn_time < codel_target_ms_) {
                codel_first_above_time_ = 0;
            } else {
                if (codel_first_above_time_ == 0) {
                    codel_first_above_time_ = now + codel_interval_ms_;
                } else if (now >= codel_first_above_time_) {
                    r.ok_to_drop = true;
                }
            }
        }
        return r;
    }

    bool pluck(std::vector<CoDelQueueItem> & value_vec) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (break_out_) {
            return false;
        }
        while (queue_.empty()) {
            cv_.wait(lock);
            if (break_out_) {
                return false;
            }
        }

        uint64_t now = Timer::GetSteadyClockMS();
        DoDequeResult r = dodeque(now);
        if(r.is_empty) {
            is_dropping_ = false;
            return true;
        }

        if(is_dropping_) {

            if (!r.ok_to_drop ) {
                is_dropping_ = 0;
            } else if (now >= codel_drop_next_) {
                while (now >= codel_drop_next_ && is_dropping_) {
                    r.item.is_drop = true;
                    value_vec.push_back(r.item);

                    ++drop_count_;

                    r = dodeque(now);
                    if (!r.ok_to_drop) {
                        is_dropping_ = 0;
                    } else {
                        codel_drop_next_ = control_law(codel_drop_next_);
                    }
                }
            }
        } else if (r.ok_to_drop &&
                ((now - codel_drop_next_ < codel_interval_ms_) ||
                 (now - codel_first_above_time_ >= codel_interval_ms_))) {

            r.item.is_drop = true;
            value_vec.push_back(r.item);

            r = dodeque(now);
            is_dropping_ = 1;

            if (now - codel_drop_next_ < codel_interval_ms_) {
                drop_count_ = drop_count_>2? drop_count_-2 : 1;
            } else {
                drop_count_ = 1;
            }
            codel_drop_next_ = control_law(now);
        }

        if(!r.is_empty) {
            r.item.is_drop = false;
            value_vec.push_back(r.item);
        }
        return true;
    }

    void break_out() {
        std::lock_guard<std::mutex> lock(mutex_);
        break_out_ = true;
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<CoDelQueueItem> queue_;
    bool break_out_;
    std::atomic_int size_;

    uint64_t codel_target_ms_;
    uint64_t codel_interval_ms_;
    int drop_count_;
    bool is_dropping_;
    uint64_t codel_first_above_time_;
    uint64_t codel_drop_next_;
};

} //namespace phxrpc
