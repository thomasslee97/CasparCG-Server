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

#include "AMCPCommand.h"
#include "amcp_command_context.h"

namespace caspar { namespace protocol { namespace amcp {

typedef std::function<std::future<std::wstring>(command_context& args)> amcp_command_impl_func;
typedef std::function<std::wstring(command_context& args)> amcp_command_impl_func2;

// TODO - these classes feel messy. at the very least, they should have their own file, and are passed in preconstructed to all of the register_commands portions
class command_context_factory
{
  public:
    command_context_factory(std::shared_ptr<amcp_command_static_context> static_context)
        : static_context_(std::move(static_context))
    {
    }

    command_context create(const command_context_simple& ctx2, const std::vector<channel_context>& channels) const
    {
        const channel_context channel = ctx2.channel_index >= 0 ? channels.at(ctx2.channel_index) : channel_context();
        auto ctx = command_context(static_context_, channels, ctx2.client, channel, ctx2.channel_index, ctx2.layer_id);
        ctx.parameters = std::move(ctx2.parameters);
        return std::move(ctx);
    }

  private:
    std::shared_ptr<amcp_command_static_context> static_context_;
};

class amcp_command_repository_wrapper
{
  public:
    amcp_command_repository_wrapper(std::shared_ptr<amcp_command_repository> repo,
                                    std::shared_ptr<command_context_factory> ctx)
        : repo_(repo)
        , ctx_(ctx)
    {
    }

    void register_command(std::wstring              category,
                          std::wstring              name,
                          core::help_item_describer describer,
                          amcp_command_impl_func    command,
                          int                       min_num_params);

    void register_command(std::wstring              category,
                          std::wstring              name,
                          core::help_item_describer describer,
                          amcp_command_impl_func2    command,
                          int                       min_num_params);

    void register_channel_command(std::wstring              category,
                                  std::wstring              name,
                                  core::help_item_describer describer,
                                  amcp_command_impl_func    command,
                                  int                       min_num_params);

    void register_channel_command(std::wstring              category,
                                  std::wstring              name,
                                  core::help_item_describer describer,
                                  amcp_command_impl_func2    command,
                                  int                       min_num_params);

    spl::shared_ptr<core::help_repository> help_repo() const;

  private:
    std::shared_ptr<amcp_command_repository> repo_;
    std::shared_ptr<command_context_factory> ctx_;
};

void register_commands(std::shared_ptr<amcp_command_repository_wrapper>& repo);

}}} // namespace caspar::protocol::amcp
