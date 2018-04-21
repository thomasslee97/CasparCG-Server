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
#include "amcp_commands_timecode.h"
#include "core/producer/timecode_source.h"
#include "core/producer/frame_producer.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace caspar { namespace protocol { namespace amcp {

std::wstring time_command(command_context& ctx)
{
    const auto ch = ctx.channel.raw_channel->timecode();

    if (ctx.parameters.size() > 0) {
        if (!ch->is_free())
            return L"4xx TIME FAILED\r\n";

        core::frame_timecode tc;
        const uint8_t        fps = static_cast<uint8_t>(round(ctx.channel.raw_channel->video_format_desc().fps));
        if (!core::frame_timecode::parse_string(ctx.parameters.at(0), fps, tc))
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
    auto timecode = core::frame_timecode::empty();
    if (!ctx.parameters.empty()) {// && !core::frame_timecode::parse_string(ctx.parameters.at(0), fps, timecode)) { // TODO - fix this
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
    const auto token = ctx.parameters.at(0);
    const auto info  = ctx.static_context->scheduler->find(token);

    if (info.first == core::frame_timecode::empty() || !info.second) {
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
    
    const std::list<std::wstring> tokens(ctx.parameters.begin() + 2, ctx.parameters.end());
    std::shared_ptr<AMCPCommand>  command =
        ctx.static_context->parser->parse_command(ctx.client, tokens, schedule_token);
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

    core::frame_timecode schedule_timecode = core::frame_timecode::empty();
    const uint8_t        fps = static_cast<uint8_t>(round(ctx.channels[channel_index].raw_channel->video_format_desc().fps));
    if (!core::frame_timecode::parse_string(ctx.parameters.at(1), fps, schedule_timecode) ||
        !schedule_timecode.is_valid()) {
        return L"403 SCHEDULE SET ERROR\r\n";
    }

    ctx.static_context->scheduler->set(channel_index, std::move(schedule_token), schedule_timecode, std::move(command));

    return L"202 SCHEDULE SET OK\r\n";
}

    // TODO - refactor this
std::wstring timecode_command(command_context& ctx)
{
    if (boost::iequals(ctx.parameters.at(0), L"CLOCK")) {
        ctx.channel.raw_channel->timecode()->set_system_time();
        
        return L"202 TIMECODE SOURCE OK\r\n";
    }
    if (boost::iequals(ctx.parameters.at(0), L"LAYER")) {
        if (ctx.parameters.size() < 2) {
            return L"202 TIMECODE SOURCE OK\r\n";
        }

        const int layer = boost::lexical_cast<int>(ctx.parameters.at(1));
        const auto producer = ctx.channel.stage->foreground(layer).share();
        ctx.channel.stage->execute([=]() { ctx.channel.raw_channel->timecode()->set_weak_source(producer.get()); });

        return L"202 TIMECODE SOURCE OK\r\n";
    }
    if (boost::iequals(ctx.parameters.at(0), L"CLEAR")) {
        ctx.channel.raw_channel->timecode()->clear_source();

        return L"202 TIMECODE SOURCE OK\r\n";
    }

    return L"400 TIMECODE SOURCE FAILED\r\n";
}

void register_timecode_commands(std::shared_ptr<amcp_command_repository_wrapper>& repo)
{
    repo->register_command(L"Scheduler Commands", L"SCHEDULE REMOVE", nullptr, schedule_remove_command, 1);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE CLEAR", nullptr, schedule_clear_command, 0);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE LIST", nullptr, schedule_list_command, 0);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE INFO", nullptr, schedule_info_command, 1);
    repo->register_command(L"Scheduler Commands", L"SCHEDULE SET", nullptr, schedule_set_command, 3);

    repo->register_channel_command(L"Timecode Commands", L"TIMECODE SOURCE", nullptr, timecode_command, 1);

    repo->register_channel_command(L"Query Commands", L"TIME", nullptr, time_command, 0);
}

}}} // namespace caspar::protocol::amcp
