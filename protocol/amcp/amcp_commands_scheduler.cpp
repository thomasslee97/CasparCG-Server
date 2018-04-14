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
 * Author:
 */

#pragma once

#include "amcp_command_context.h"
#include "amcp_commands_scheduler.h"

namespace caspar { namespace protocol { namespace amcp {

std::wstring time_command(command_context& ctx)
{
    const auto ch = ctx.channel.raw_channel->timecode();

    if (ctx.parameters.size() > 0) 
    {
        if (!ch->is_free())
            return L"4xx TIME FAILED\r\n";

        core::frame_timecode tc;
        if (!core::frame_timecode::parse_string(ctx.parameters.at(0), tc))
            return L"4xx TIME FAILED\r\n";

        ch->timecode(tc);
    }

    std::wstringstream replyString;
    replyString << L"201 TIME OK\r\n";
    replyString << ch->timecode().string();
    replyString << L"\r\n";
    return replyString.str();
}

std::wstring schedule_remove_command(command_context& ctx)
{
    const std::wstring token = ctx.parameters.at(0);
    if (!ctx.static_context->scheduler->remove(token)) {
        return L"403 SCHEDULE REMOVE ERROR\r\n";
    }

    return L"202 SCHEDULE REMOVE OK\r\n";
}

std::wstring schedule_clear_command(command_context& ctx)
{
    ctx.static_context->scheduler->clear();
    return L"202 SCHEDULE CLEAR OK\r\n";
}

std::wstring schedule_list_command(command_context& ctx)
{
    auto timecode = core::frame_timecode::get_default();
    if (!ctx.parameters.empty() && !core::frame_timecode::parse_string(ctx.parameters.at(0), timecode)) {
        return L"403 SCHEDULE LIST ERROR\r\n";
    }

    std::wstringstream replyString;
    replyString << L"200 SCHEDULE LIST OK\r\n";

    for (auto entry : ctx.static_context->scheduler->list(timecode)) {
        replyString << entry.first.string() << L" " << entry.second << "\r\n";
    }

    replyString << L"\r\n";
    return replyString.str();
}

std::wstring schedule_info_command(command_context& ctx)
{
    const auto token    = ctx.parameters.at(0);
    const auto info = ctx.static_context->scheduler->find(token);

    if (info.first == core::frame_timecode::get_default() || !info.second) {
        return L"403 SCHEDULE INFO ERROR\r\n";
    }

    std::wstringstream replyString;
    replyString << L"201 SCHEDULE INFO OK\r\n";
    replyString << info.first.string();
    replyString << L"\r\n";
    return replyString.str();
}

std::wstring schedule_set_command(command_context& ctx)
{
    std::wstring schedule_token = ctx.parameters.at(0);

    core::frame_timecode schedule_timecode = core::frame_timecode::get_default();
    if (!core::frame_timecode::parse_string(ctx.parameters.at(1), schedule_timecode) ||
        /*!schedule_timecode.is_valid() ||*/ schedule_timecode == core::frame_timecode::get_default()) {
        return L"403 SCHEDULE SET ERROR\r\n";
    }

    const std::list<std::wstring> tokens(ctx.parameters.begin() + 2, ctx.parameters.end());
    std::shared_ptr<AMCPCommand> command = ctx.static_context->parser->parse_command(ctx.client, tokens, schedule_token);
    if (!command) {
        return L"403 SCHEDULE SET ERROR\r\n";
    }

    const int channel_index = command->channel_index();
    if (!ctx.static_context->parser->check_channel_lock(ctx.client, channel_index)) {
        return L"503 SCHEDULE SET FAILED\r\n";
    }

    if (channel_index < 0) {
        // Only channel commands can be scheduled
        return L"503 SCHEDULE SET FAILED\r\n";
    }

    ctx.static_context->scheduler->set(channel_index, std::move(schedule_token), schedule_timecode, std::move(command));

    return L"202 SCHEDULE SET OK\r\n";
}

void register_scheduler_commands(std::shared_ptr<amcp_command_repository_wrapper>& repo)
{
    repo->register_command(L"Scheduler Commands", L"SCHEDULE REMOVE", nullptr, schedule_remove_command, 1);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE CLEAR", nullptr, schedule_clear_command, 0);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE LIST", nullptr, schedule_list_command, 0);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE INFO", nullptr, schedule_info_command, 1);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE SET", nullptr, schedule_set_command, 3);

    repo->register_channel_command(L"Query Commands", L"TIME", nullptr, time_command, 0);
}

}}} // namespace caspar::protocol::amcp
