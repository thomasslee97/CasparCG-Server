/*
 * Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
 *
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Nicklas P Andersson
 */

#include "../StdAfx.h"

#include "AMCPCommand.h"
#include "AMCPCommandQueue.h"
#include "AMCPCommandScheduler.h"

namespace caspar { namespace protocol { namespace amcp {

class AMCPScheduledCommand
{
  private:
    core::frame_timecode                                     timecode_;
    std::map<std::wstring, std::shared_ptr<AMCPCommandBase>> commands_{};

  public:
    AMCPScheduledCommand(const std::shared_ptr<AMCPCommandBase> command,
                         core::frame_timecode                   timecode,
                         std::wstring                           token)
        : timecode_(timecode)
    {
        commands_.insert({std::move(token), std::move(command)});
    }

    void add(const std::wstring& token, std::shared_ptr<AMCPCommandBase> command)
    {
        commands_.insert({token, command});
    }

    bool try_pop_token(const std::wstring& token)
    {
        const auto v   = commands_.find(token);
        const bool res = v != commands_.end();

        if (res)
            commands_.erase(v);

        return res;
    }

    // TODO - will this be needed later?
    bool has_passed_timecode(core::frame_timecode& now) const
    {
        // TODO - account for dst/clock changing.

        return timecode_ < now;
    }

    std::shared_ptr<AMCPBatchCommand> create_command() const
    {
        std::vector<std::shared_ptr<AMCPCommandBase>> cmds;
        for (const auto cmd : commands_) {
            cmds.push_back(cmd.second);
        }

        return std::move(std::make_shared<AMCPBatchCommand>(cmds, L""));
    }

    std::vector<std::pair<core::frame_timecode, std::wstring>> get_tokens()
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        for (const auto cmd : commands_) {
            res.push_back({timecode_, cmd.first});
        }

        return res;
    }

    core::frame_timecode timecode() const { return timecode_; }
};

/*
struct scheduler_queue_node // TODO - fold command into this type
{
    std::shared_ptr<AMCPScheduledCommand> command;
    scheduler_queue_node*                 next;
};

class scheduler_queue_list
{
  private:
    scheduler_queue_node* head;
    scheduler_queue_node* tail;
    core::frame_timecode  current_timecode;

  public:
    scheduler_queue_list()
        : head(nullptr)
        , tail(nullptr)
        , current_timecode(core::frame_timecode::get_default())
    {
    }

    void insert(std::shared_ptr<AMCPScheduledCommand> cmd)
    {
        scheduler_queue_node* prev = nullptr;
        scheduler_queue_node* node = head;
        while (node != nullptr) {
            




        }
    }
};*/

class AMCPCommandSchedulerQueue
{
  public:
    AMCPCommandSchedulerQueue(std::shared_ptr<core::channel_timecode> channel_timecode)
        : channel_timecode_(channel_timecode)
        , scheduled_commands_()
    {
    }

    void set(const std::wstring& token, const core::frame_timecode& timecode, std::shared_ptr<AMCPCommandBase> command)
    {
        if (!command || token.empty() || timecode == core::frame_timecode::get_default())
            return;

        // TODO - optimise once queue type has changed
        for (auto cmd : scheduled_commands_) {
            if (cmd->timecode() != timecode)
                continue;

            cmd->add(token, command);
        }

        // No match, so queue command instead
        auto sch_cmd = std::make_shared<AMCPScheduledCommand>(command, timecode, token);
        scheduled_commands_.push_back(std::move(sch_cmd));
    }

    bool remove(const std::wstring& token)
    {
        if (token.empty())
            return false;

        for (auto cmd : scheduled_commands_) {
            if (cmd->try_pop_token(token))
                return true;

            // TODO - should propbably discard this scheduled_command if it is empty
        }

        return false;
    }

    void clear() { scheduled_commands_.clear(); }

    std::vector<std::pair<core::frame_timecode, std::wstring>> list(core::frame_timecode& timecode)
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        const bool include_all = timecode == core::frame_timecode::get_default();

        for (auto command : scheduled_commands_) {
            for (auto token : command->get_tokens()) {
                if (include_all || timecode == token.first)
                    res.push_back(std::move(token));
            }
        }

        return res;
    }

    int schedule(std::shared_ptr<AMCPCommandQueue> dest)
    {
        int                        count = 0;
        const core::frame_timecode now   = channel_timecode_->timecode();

        // TODO - optimise once queue type has changed
        for (int i = 0; i < scheduled_commands_.size(); i++) {
            const auto cmd = scheduled_commands_[i];
            if (cmd->timecode() < now) {
                dest->AddCommand(std::move(cmd->create_command()));
                scheduled_commands_.erase(scheduled_commands_.begin() + i);
                --i;
                count++;
            }
        }

        return count;
    }

    std::shared_ptr<core::channel_timecode> channel_timecode() const { return channel_timecode_; }

  private:
    std::shared_ptr<core::channel_timecode> channel_timecode_;
    // TODO - this should be something sorted. it will make insertion more costly, but then finding and removing items
    // will be a lot cheaper. would increase cost + complexty, and performance may not be an issue
    std::vector<std::shared_ptr<AMCPScheduledCommand>> scheduled_commands_;
};

struct AMCPCommandScheduler::Impl
{
  private:
    std::vector<std::shared_ptr<AMCPCommandSchedulerQueue>> queues_;
    std::timed_mutex                                        lock_;

  public:
    void add_channel(std::shared_ptr<core::channel_timecode> channel_timecode)
    {
        queues_.push_back(std::make_shared<AMCPCommandSchedulerQueue>(channel_timecode));
    }

    void set(int                              channel_index,
             const std::wstring&              token,
             const core::frame_timecode&      timecode,
             std::shared_ptr<AMCPCommandBase> command)
    {
        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            queue->remove(token);
        }

        queues_.at(channel_index)->set(token, timecode, std::move(command));
    }

    bool remove(const std::wstring& token)
    {
        if (token.empty())
            return false;

        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            if (queue->remove(token))
                return true;
        }

        return false;
    }

    void clear()
    {
        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            queue->clear();
        }
    }

    std::vector<std::pair<core::frame_timecode, std::wstring>> list(core::frame_timecode& timecode)
    {
        std::vector<std::pair<core::frame_timecode, std::wstring>> res;

        std::lock_guard<std::timed_mutex> lock(lock_);

        for (auto queue : queues_) {
            for (auto token : queue->list(timecode)) {
                res.push_back(std::move(token));
            }
        }

        return res;
    }

    class timeout_lock
    {
      private:
        std::timed_mutex& mutex_;
        bool              locked_;

      public:
        timeout_lock(std::timed_mutex& mutex, int milliseconds)
            : mutex_(mutex)
        {
            locked_ = mutex_.try_lock_for(std::chrono::milliseconds(milliseconds));
        }
        ~timeout_lock() { mutex_.unlock(); }

        bool is_locked() const { return locked_; }
    };

    int schedule(int channel_index, std::shared_ptr<AMCPCommandQueue> dest)
    {
        // TODO - tweak this timeout
        timeout_lock lock(lock_, 5);

        if (!lock.is_locked())
            return -1;

        return queues_.at(channel_index)->schedule(dest);
    }
};

AMCPCommandScheduler::AMCPCommandScheduler()
    : impl_(new Impl())
{
}

void AMCPCommandScheduler::add_channel(std::shared_ptr<core::channel_timecode> channel_timecode)
{
    impl_->add_channel(channel_timecode);
}

void AMCPCommandScheduler::set(int                              channel_index,
                               const std::wstring&              token,
                               const core::frame_timecode&      timecode,
                               std::shared_ptr<AMCPCommandBase> command)
{
    impl_->set(channel_index, token, timecode, command);
}

bool AMCPCommandScheduler::remove(const std::wstring& token) { return impl_->remove(token); }

void AMCPCommandScheduler::clear() { return impl_->clear(); }

std::vector<std::pair<core::frame_timecode, std::wstring>> AMCPCommandScheduler::list(core::frame_timecode& timecode)
{
    return impl_->list(timecode);
}

int AMCPCommandScheduler::schedule(int channel_index, std::shared_ptr<AMCPCommandQueue> dest)
{
    return impl_->schedule(channel_index, dest);
}

}}} // namespace caspar::protocol::amcp
