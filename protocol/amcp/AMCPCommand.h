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
#include "AMCPCommandScheduler.h"
#include "amcp_shared.h"
#include <core/consumer/frame_consumer.h>
#include <core/producer/frame_producer.h>
#include <core/producer/stage.h>

#include <core/video_channel.h>
#include <core/thumbnail_generator.h>
#include <core/producer/media_info/media_info_repository.h>
#include <core/producer/cg_proxy.h>
#include <core/system_info_provider.h>

namespace caspar { namespace protocol { namespace amcp {

class amcp_command_parser
{
  public:
    virtual ~amcp_command_parser() = default;

    virtual std::shared_ptr<AMCPCommandBase>
    parse_command(IO::ClientInfoPtr client, std::list<std::wstring> tokens, const std::wstring& request_id) const = 0;

    virtual bool check_channel_lock(IO::ClientInfoPtr client, int channel_index) const = 0;
};

struct amcp_command_registry_context
{
    const std::vector<channel_context>                            channels;
    const std::shared_ptr<core::thumbnail_generator>             thumb_gen;
    const spl::shared_ptr<core::media_info_repository>           media_info_repo;
    const spl::shared_ptr<core::system_info_provider_repository> system_info_provider_repo;
    const spl::shared_ptr<core::cg_producer_registry>             cg_registry;
    const spl::shared_ptr<core::help_repository>                 help_repo;
    const spl::shared_ptr<const core::frame_producer_registry>    producer_registry;
    const spl::shared_ptr<const core::frame_consumer_registry>    consumer_registry;
    const spl::shared_ptr<AMCPCommandScheduler>                   scheduler;
    const std::shared_ptr<amcp_command_parser>                    parser;
    const std::shared_ptr<accelerator::ogl::device>              ogl_device;
    std::promise<bool>&                                      shutdown_server_now;

    amcp_command_registry_context(
        const std::vector<channel_context>                            channels,
        const std::shared_ptr<core::thumbnail_generator>&             thumb_gen,
        const spl::shared_ptr<core::media_info_repository>&           media_info_repo,
        const spl::shared_ptr<core::system_info_provider_repository>& system_info_provider_repo,
        const spl::shared_ptr<core::cg_producer_registry>             cg_registry,
        const spl::shared_ptr<core::help_repository>&                 help_repo,
        const spl::shared_ptr<const core::frame_producer_registry>    producer_registry,
        const spl::shared_ptr<const core::frame_consumer_registry>    consumer_registry,
        const spl::shared_ptr<AMCPCommandScheduler>                   scheduler,
        const std::shared_ptr<amcp_command_parser>                    parser,
        const std::shared_ptr<accelerator::ogl::device>&              ogl_device,
        std::promise<bool>&                                     shutdown_server_now)
        : channels(std::move(channels))
        , thumb_gen(std::move(thumb_gen))
        , media_info_repo(std::move(media_info_repo))
        , system_info_provider_repo(std::move(system_info_provider_repo))
        , cg_registry(std::move(cg_registry))
        , help_repo(std::move(help_repo))
        , producer_registry(std::move(producer_registry))
        , consumer_registry(std::move(consumer_registry))
        , scheduler(std::move(scheduler))
        , parser(std::move(parser))
        , ogl_device(std::move(ogl_device))
        , shutdown_server_now(shutdown_server_now)
    {
    }
};

struct command_context
{
    const amcp_command_registry_context static_context;
    const IO::ClientInfoPtr             client;
    const channel_context               channel;
    const int                           channel_index;
    const int                           layer_id;
    std::vector<std::wstring>           parameters;

    int layer_index(int default_ = 0) const { return layer_id == -1 ? default_ : layer_id; }

    command_context(const amcp_command_registry_context& static_context,
                    IO::ClientInfoPtr                   client,
                    channel_context                     channel,
                    int                                 channel_index,
                    int                                 layer_id)
        : static_context(std::move(static_context))
        , client(std::move(client))
        , channel(channel)
        , channel_index(channel_index)
        , layer_id(layer_id)
    {
    }

    std::vector<channel_context> channels() const { return static_context.channels; }
};

typedef std::function<std::wstring(command_context& args)> amcp_command_func;

void inline send_reply_inner(const std::shared_ptr<IO::client_connection<wchar_t>> client,
                             const std::wstring&                                   req_id,
                             const std::wstring&                                   str)
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
    command_context         ctx_;
    const amcp_command_func command_;
    const std::wstring      name_;
    const std::wstring      request_id_;

  public:
    AMCPCommand(const command_context&           ctx,
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
                std::unique_lock<std::mutex> lock = ctx_.channel.channel->stage().get_lock();
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
