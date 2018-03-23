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

#pragma once

#include "../util/ClientInfo.h"
#include "AMCPCommandBase.h"
#include "amcp_shared.h"
#include <core/consumer/frame_consumer.h>
#include <core/producer/frame_producer.h>

#include <core/producer/cg_proxy.h>

namespace caspar { namespace protocol { namespace amcp {
    
void inline send_reply_inner(const IO::ClientInfoPtr client, const std::wstring& req_id, const std::wstring& str)
{
    if (str.empty())
        return;

    std::wstring reply = str;
    if (!req_id.empty())
        reply = L"RES " + req_id + L" " + str;

    client->send(std::move(reply));
}

// TODO: AMCPCommandBase replace with AMCPCommand again? perhaps it will need an impl struct to make it work?

class AMCPCommand : public AMCPCommandBase // TODO AMCPSubCommand
{
  private:
    command_context_simple  ctx_;
    const amcp_command_func command_;
    const std::wstring      name_;
    const std::wstring      request_id_;

  public:
    AMCPCommand(const command_context_simple&    ctx,
                const amcp_command_func&         command,
                const std::wstring&              name,
                const std::wstring&              request_id,
                const std::vector<std::wstring>& parameters)
        : ctx_(ctx)
        , command_(command)
        , name_(name)
        , request_id_(request_id)
    {
        ctx_.parameters = parameters;
    }

    typedef std::shared_ptr<AMCPCommand> ptr_type;

    bool Execute() override
    {
        // TODO - dont reply if part of batch and no req_id
        std::wstring res;
        { // TODO - move lock to batched command only
            if (ctx_.channel_index >= 0) {
                //std::unique_lock<std::mutex> lock = ctx.channel.channel->stage().get_lock(); // TODO - reenable
                res                               = command_(ctx_);
            } else {
                res = command_(ctx_);
            }
        }

        SendReply(res);

        return true;
    }

    void SendReply(const std::wstring& str) const override { send_reply_inner(ctx_.client, request_id_, str); }

    IO::ClientInfoPtr client() const { return ctx_.client; }

    std::wstring name() const override { return name_; }

    int channel_index() const override { return ctx_.channel_index; }
};

class AMCPBatchCommand : public AMCPCommandBase // TODO AMCPCommand
{
    const std::vector<std::shared_ptr<AMCPCommandBase>> commands_;
    const std::wstring                                  request_id_;

  public:
    AMCPBatchCommand(const std::vector<std::shared_ptr<AMCPCommandBase>> commands, const std::wstring& request_id)
        : commands_(commands)
        , request_id_(request_id)
    {
        // TODO - needs a client, to send responses to, if needed. when scheduled, the response should be sent to the
        // child commands clients?
    }

    bool Execute() override
    {
        if (commands_.size() == 0) {
            return false;
        }

        std::set<int> channels;
        for (const auto cmd : commands_) {
            channels.insert(cmd->channel_index());
        }

        /*
        command_context ctx; // TODO where to get it from

        // TODO - this will not work if a batch inside a batch
        // TODO - this will not handle swap commands at all well
        std::vector<std::unique_lock<std::mutex>> locks;
        // This runs sequentially through channel indixes to remove chance of race conditions if multiple run at once
        for (const int ch : channels) {
            if (ch < 0)
                continue;

            locks.push_back(ctx.channels().at(ch).channel->stage().get_lock());
        }*/

        // TODO - guard against a recursive circle of commands (just in case)

        for (auto cmd : commands_) {
            cmd->Execute();
        }

        // TODO - send success reply if request id?

        return true;
    }

    void SendReply(const std::wstring& str) const override
    {
        // Already logged to console by AMCPCommandQueue
        // TODO - send reply if has request id?
    }

    std::wstring name() const override { return L"BATCH"; } // TODO include count

    int channel_index() const override { return -1; } // TODO remove or make useful for chaining
};

}}} // namespace caspar::protocol::amcp
