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
 * Author: Helge Norberg, helge.norberg@svt.se
 */

#include "../StdAfx.h"

#include "amcp_command_repository.h"

#include <common/env.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>

namespace caspar { namespace protocol { namespace amcp {

AMCPCommand::ptr_type find_command(const std::map<std::wstring, std::pair<amcp_command_func, int>>& commands,
                                   const std::wstring&                                              name,
                                   const std::wstring&                                              id,
                                   const command_context&                                           ctx,
                                   std::list<std::wstring>&                                         tokens)
{
    std::wstring subcommand;

    if (!tokens.empty())
        subcommand = boost::to_upper_copy(tokens.front());

    // Start with subcommand syntax like MIXER CLEAR etc
    if (!subcommand.empty()) {
        auto       s      = name + L" " + subcommand;
        const auto subcmd = commands.find(s);

        if (subcmd != commands.end()) {
            tokens.pop_front();

            if (tokens.size() >= subcmd->second.second) {
                const std::vector<std::wstring> parameters(tokens.begin(), tokens.end());

                return std::make_shared<AMCPCommand>(ctx, subcmd->second.first, s, id, std::move(parameters));
            }
        }
    }

    // Resort to ordinary command
    const auto command = commands.find(name);

    if (command != commands.end() && tokens.size() >= command->second.second) {
        const std::vector<std::wstring> parameters(tokens.begin(), tokens.end());

        return std::make_shared<AMCPCommand>(ctx, command->second.first, name, id, std::move(parameters));
    }

    return nullptr;
}

std::vector<channel_context> build_channel_contexts(const std::vector<spl::shared_ptr<core::video_channel>>& channels)
{
    std::vector<channel_context> res;

    int index = 0;
    for (const auto& channel : channels) {
        const std::wstring lifecycle_key = L"lock" + boost::lexical_cast<std::wstring>(index);
        res.push_back(channel_context(channel, lifecycle_key));
        ++index;
    }

    return res;
}

template <typename Out, typename In>
bool try_lexical_cast(const In& input, Out& result)
{
    Out        saved   = result;
    const bool success = boost::conversion::detail::try_lexical_convert(input, result);

    if (!success)
        result = saved; // Needed because of how try_lexical_convert is implemented.

    return success;
}

static void
parse_channel_id(std::list<std::wstring>& tokens, std::wstring& channel_spec, int& channel_index, int& layer_index)
{
    if (!tokens.empty()) {
        channel_spec                            = tokens.front();
        std::wstring              channelid_str = boost::trim_copy(channel_spec);
        std::vector<std::wstring> split;
        boost::split(split, channelid_str, boost::is_any_of("-"));

        // Use non_throwing lexical cast to not hit exception break point all the time.
        if (try_lexical_cast(split[0], channel_index)) {
            --channel_index;

            if (split.size() > 1)
                try_lexical_cast(split[1], layer_index);

            // Consume channel-spec
            tokens.pop_front();
        }
    }
}

struct amcp_command_repository::impl
    : amcp_command_parser
    , std::enable_shared_from_this<amcp_command_parser>
{
    const amcp_command_registry_context context_;

    std::map<std::wstring, std::pair<amcp_command_func, int>> commands{};
    std::map<std::wstring, std::pair<amcp_command_func, int>> channel_commands{};

    impl(const std::vector<spl::shared_ptr<core::video_channel>>&      channels,
         const std::shared_ptr<core::thumbnail_generator>&             thumb_gen,
         spl::shared_ptr<core::media_info_repository>&           media_info_repo,
         const spl::shared_ptr<core::system_info_provider_repository>& system_info_provider_repo,
         const spl::shared_ptr<core::cg_producer_registry>&            cg_registry,
         const spl::shared_ptr<core::help_repository>&                 help_repo,
         const spl::shared_ptr<const core::frame_producer_registry>&   producer_registry,
         const spl::shared_ptr<const core::frame_consumer_registry>&   consumer_registry,
         const spl::shared_ptr<AMCPCommandScheduler>&                  scheduler,
         const std::shared_ptr<accelerator::ogl::device>&              ogl_device,
         std::promise<bool>&                                           shutdown_server_now)
        : context_(build_channel_contexts(channels),
                   thumb_gen,
                   media_info_repo,
                   system_info_provider_repo,
                   cg_registry,
                   help_repo,
                   producer_registry,
                   consumer_registry,
                   scheduler,
                   std::shared_ptr<amcp_command_parser>(this),
                   ogl_device,
                   shutdown_server_now)
    {
    }

    AMCPCommand::ptr_type create_command(const std::wstring&      name,
                                         const std::wstring&      id,
                                         IO::ClientInfoPtr        client,
                                         std::list<std::wstring>& tokens) const
    {
        const command_context ctx(context_, std::move(client), channel_context(), -1, -1);

        auto command = find_command(commands, name, id, ctx, tokens);

        if (command)
            return command;

        return nullptr;
    }

    AMCPCommand::ptr_type create_channel_command(const std::wstring&      name,
                                                 const std::wstring&      id,
                                                 IO::ClientInfoPtr        client,
                                                 unsigned int             channel_index,
                                                 int                      layer_index,
                                                 std::list<std::wstring>& tokens) const
    {
        const auto channel = context_.channels.at(channel_index);

        const command_context ctx(context_, std::move(client), channel, channel_index, layer_index);

        auto command = find_command(channel_commands, name, id, ctx, tokens);

        if (command)
            return command;

        return nullptr;
    }

    std::shared_ptr<AMCPCommandBase> parse_command(IO::ClientInfoPtr       client,
                                                   std::list<std::wstring> tokens,
                                                   const std::wstring&     request_id) const override
    {
        // Consume command name
        const std::basic_string<wchar_t> command_name = boost::to_upper_copy(tokens.front());
        tokens.pop_front();

        // Determine whether the next parameter is a channel spec or not
        int          channel_index = -1;
        int          layer_index   = -1;
        std::wstring channel_spec;
        parse_channel_id(tokens, channel_spec, channel_index, layer_index);

        // Create command instance
        std::shared_ptr<AMCPCommand> command;
        if (channel_index >= 0) {
            command = create_channel_command(command_name, request_id, client, channel_index, layer_index, tokens);

            if (!command) // Might be a non channel command, although the first argument is numeric
            {
                // Restore backed up channel spec string.
                tokens.push_front(channel_spec);
            }
        }

        // Create global instance
        if (!command) {
            command = create_command(command_name, request_id, client, tokens);
        }

        return std::move(command);
    }

    bool check_channel_lock(IO::ClientInfoPtr client, int channel_index) const override
    {
        if (channel_index < 0)
            return true;

        auto lock = context_.channels.at(channel_index).lock;
        return !(lock && !lock->check_access(client));
    }
};

amcp_command_repository::amcp_command_repository(
    const std::vector<spl::shared_ptr<core::video_channel>>&      channels,
    const std::shared_ptr<core::thumbnail_generator>&             thumb_gen,
    spl::shared_ptr<core::media_info_repository>&           media_info_repo,
    const spl::shared_ptr<core::system_info_provider_repository>& system_info_provider_repo,
    const spl::shared_ptr<core::cg_producer_registry>&            cg_registry,
    const spl::shared_ptr<core::help_repository>&                 help_repo,
    const spl::shared_ptr<const core::frame_producer_registry>&   producer_registry,
    const spl::shared_ptr<const core::frame_consumer_registry>&   consumer_registry,
    const spl::shared_ptr<AMCPCommandScheduler>&                  scheduler,
    const std::shared_ptr<accelerator::ogl::device>&              ogl_device,
    std::promise<bool>&                                           shutdown_server_now)
    : impl_(new impl(channels,
                     thumb_gen,
                     media_info_repo,
                     system_info_provider_repo,
                     cg_registry,
                     help_repo,
                     producer_registry,
                     consumer_registry,
                     scheduler,
                     ogl_device,
                     shutdown_server_now))
{
}

const std::vector<channel_context>& amcp_command_repository::channels() const { return impl_->context_.channels; }

std::shared_ptr<AMCPCommandBase> amcp_command_repository::parse_command(IO::ClientInfoPtr       client,
                                                                        std::list<std::wstring> tokens,
                                                                        const std::wstring&     request_id) const
{
    return impl_->parse_command(client, tokens, request_id);
}

bool amcp_command_repository::check_channel_lock(IO::ClientInfoPtr client, int channel_index) const
{
    return impl_->check_channel_lock(client, channel_index);
}

void amcp_command_repository::register_command(std::wstring              category,
                                               std::wstring              name,
                                               core::help_item_describer describer,
                                               amcp_command_func         command,
                                               int                       min_num_params)
{
    if (describer)
        impl_->context_.help_repo->register_item({ L"AMCP", category }, name, describer);

    impl_->commands.insert(std::make_pair(std::move(name), std::make_pair(std::move(command), min_num_params)));
}

void amcp_command_repository::register_channel_command(std::wstring              category,
                                                       std::wstring              name,
                                                       core::help_item_describer describer,
                                                       amcp_command_func         command,
                                                       int                       min_num_params)
{
    if (describer)
        impl_->context_.help_repo->register_item({L"AMCP", category}, name, describer);

    impl_->channel_commands.insert(std::make_pair(std::move(name), std::make_pair(std::move(command), min_num_params)));
}

spl::shared_ptr<core::help_repository> amcp_command_repository::help_repo() const { return impl_->context_.help_repo; }

}}} // namespace caspar::protocol::amcp
