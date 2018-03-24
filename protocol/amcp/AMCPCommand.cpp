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

#include "../StdAfx.h"

#include "AMCPCommand.h"
#include "common/timer.h"
#include <boost/lexical_cast.hpp>
#include <core/producer/stage.h>

namespace caspar { namespace protocol { namespace amcp {

bool cmd_exec(bool batch_has_client, std::shared_ptr<AMCPCommand> pCurrentCommand)
{
    try {
        try {
            caspar::timer timer;

            auto name = pCurrentCommand->name();
            CASPAR_LOG(debug) << "Executing command: " << name;

            pCurrentCommand->Execute(batch_has_client);

            CASPAR_LOG(debug) << "Executed command (" << timer.elapsed() << "s): " << name;
            return true;

        } catch (file_not_found&) {
            CASPAR_LOG(error) << " Turn on log level debug for stacktrace.";
            pCurrentCommand->SendReply(L"404 " + pCurrentCommand->name() + L" FAILED\r\n");
        } catch (expected_user_error&) {
            pCurrentCommand->SendReply(L"403 " + pCurrentCommand->name() + L" FAILED\r\n");
        } catch (user_error&) {
            CASPAR_LOG(error) << " Check syntax. Turn on log level debug for stacktrace.";
            pCurrentCommand->SendReply(L"403 " + pCurrentCommand->name() + L" FAILED\r\n");
        } catch (std::out_of_range&) {
            CASPAR_LOG(error) << L"Missing parameter. Check syntax. Turn on log level debug for stacktrace.";
            pCurrentCommand->SendReply(L"402 " + pCurrentCommand->name() + L" FAILED\r\n");
        } catch (boost::bad_lexical_cast&) {
            CASPAR_LOG(error) << L"Invalid parameter. Check syntax. Turn on log level debug for stacktrace.";
            pCurrentCommand->SendReply(L"403 " + pCurrentCommand->name() + L" FAILED\r\n");
        } catch (...) {
            CASPAR_LOG_CURRENT_EXCEPTION();
            CASPAR_LOG(error) << "Failed to execute command:" << pCurrentCommand->name();
            pCurrentCommand->SendReply(L"501 " + pCurrentCommand->name() + L" FAILED\r\n");
        }

    } catch (...) {
        CASPAR_LOG_CURRENT_EXCEPTION();
    }

    return false;
}

void AMCPCommand::Execute(bool reply_without_req_id)
{
    const std::wstring res = command_(ctx_);

    if (reply_without_req_id || !request_id_.empty()) {
        SendReply(res);
    }
}

void send_reply(IO::ClientInfoPtrStd client, const std::wstring& str, const std::wstring& request_id)
{
    if (str.empty())
        return;

    std::wstring reply = str;
    if (!request_id.empty())
        reply = L"RES " + request_id + L" " + str;

    client->send(std::move(reply));
}

void AMCPCommand::SendReply(const std::wstring& str) const { send_reply(ctx_.client, str, request_id_); }

bool AMCPGroupCommand::Execute(const std::vector<channel_context>& channels_ctx) const
{
    caspar::timer timer;

    const bool is_batch = commands_.size() > 1;

    if (is_batch) {
        CASPAR_LOG(debug) << "Executing command: " << name();
    }

    if (commands_.size() == 0) {
        return false;
    }

    std::set<int> channels;
    for (const auto cmd : commands_) {
        channels.insert(cmd->channel_index());
    }

    // TODO - this does not handle swap commands at all well
    std::vector<std::unique_lock<std::mutex>> locks;
    // This runs sequentially through channel indixes to remove chance of race conditions if multiple run at once
    if (is_batch) { // TODO - can this be done with causing threading issues?
        for (const int ch : channels) {
            if (ch < 0 || ch >= channels_ctx.size())
                continue;

            locks.push_back(channels_ctx.at(ch).channel->stage().get_lock());
        }
    }

    int failed_count = 0;

    const bool has_client = !!client_;
    for (const auto cmd : commands_) {
        if (!cmd_exec(has_client || !is_batch, cmd)) {
            failed_count++;
        }
    }

    locks.clear();

    if (client_) {
        // TODO - report failures?
        send_reply(client_, L"202 COMMIT OK\r\n", request_id_);
    }

    CASPAR_LOG(debug) << "Executed command (" << timer.elapsed() << "s): " << name();

    return true;
}

void AMCPGroupCommand::SendReply(const std::wstring& str) const
{
    if (client_) {
        send_reply(client_, str, request_id_);
        return;
    }

    if (commands_.size() == 1) {
        commands_.at(0)->SendReply(str);
    }

}

std::wstring AMCPGroupCommand::name() const
{
    if (commands_.size() == 1) {
        return commands_.at(0)->name();
    }

    return L"BATCH"; // TODO include count
}

}}} // namespace caspar::protocol::amcp
