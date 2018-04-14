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

#include "AMCPCommandQueue.h"

#include "common/timer.h"
#include "core/producer/stage.h"
#include <boost/lexical_cast.hpp>

namespace caspar { namespace protocol { namespace amcp {

AMCPCommandQueue::AMCPCommandQueue(const std::wstring& name, const std::vector<channel_context>& channels)
    : executor_(L"AMCPCommandQueue " + name)
    , channels_(channels)
{
}

bool exec_cmd(std::shared_ptr<AMCPCommand> cmd, const std::vector<channel_context>& channels, bool reply_without_req_id)
{
    try {
        try {
            caspar::timer timer;

            auto name = cmd->name();
            CASPAR_LOG(warning) << "Executing command: " << name;

            cmd->Execute(channels, reply_without_req_id);

            CASPAR_LOG(warning) << "Executed command (" << timer.elapsed() << "s): " << name;
            return true;

        } catch (file_not_found&) {
            CASPAR_LOG(error) << " Turn on log level debug for stacktrace.";
            cmd->SendReply(L"404 " + cmd->name() + L" FAILED\r\n");
        } catch (expected_user_error&) {
            cmd->SendReply(L"403 " + cmd->name() + L" FAILED\r\n");
        } catch (user_error&) {
            CASPAR_LOG(error) << " Check syntax. Turn on log level debug for stacktrace.";
            cmd->SendReply(L"403 " + cmd->name() + L" FAILED\r\n");
        } catch (std::out_of_range&) {
            CASPAR_LOG(error) << L"Missing parameter. Check syntax. Turn on log level debug for stacktrace.";
            cmd->SendReply(L"402 " + cmd->name() + L" FAILED\r\n");
        } catch (boost::bad_lexical_cast&) {
            CASPAR_LOG(error) << L"Invalid parameter. Check syntax. Turn on log level debug for stacktrace.";
            cmd->SendReply(L"403 " + cmd->name() + L" FAILED\r\n");
        } catch (...) {
            CASPAR_LOG_CURRENT_EXCEPTION();
            CASPAR_LOG(error) << "Failed to execute command:" << cmd->name();
            cmd->SendReply(L"501 " + cmd->name() + L" FAILED\r\n");
        }

    } catch (...) {
        CASPAR_LOG_CURRENT_EXCEPTION();
    }

    return false;
}

void AMCPCommandQueue::AddCommand(std::shared_ptr<AMCPGroupCommand> pCurrentCommand)
{
    if (!pCurrentCommand)
        return;

    if (executor_.size() > 128) {
        try {
            CASPAR_LOG(error) << "AMCP Command Queue Overflow.";
            CASPAR_LOG(error) << "Failed to execute command:" << pCurrentCommand->name();
            pCurrentCommand->SendReply(L"500 FAILED\r\n");
        } catch (...) {
            CASPAR_LOG_CURRENT_EXCEPTION();
        }
    }

    executor_.begin_invoke([=] {
        try {
            Execute(pCurrentCommand);
            // pCurrentCommand->Execute(channels_);

            CASPAR_LOG(trace) << "Ready for a new command";
        } catch (...) {
            CASPAR_LOG_CURRENT_EXCEPTION();
        }
    });
}

void AMCPCommandQueue::Execute(std::shared_ptr<AMCPGroupCommand> cmd) const 
{
    if (cmd->Commands().empty())
        return;

    caspar::timer timer;
    CASPAR_LOG(warning) << "Executing command: " << cmd->name();

    if (cmd->Commands().size() == 1) {
        exec_cmd(cmd->Commands().at(0), channels_, true);

        CASPAR_LOG(warning) << "Executed command (" << timer.elapsed() << "s): " << cmd->name();
        return;
    }

    // TODO - need to make sure we clean up properly, in case of an exception in here somewhere?
    std::vector<channel_context>               channels2;
    std::vector<std::shared_ptr<core::stage_delayed>> stages2;
    for (auto& ch : channels_) {
        std::promise<void> wa;

        auto st = std::make_shared<core::stage_delayed>(ch.raw_channel->stage());
        stages2.push_back(st);
        channels2.emplace_back(ch.raw_channel, st, ch.lifecycle_key_);
    }

    int failed = 0; // TODO - should this be futures?

    // now 'execute' aka queue all comamnds
    for (auto& cmd2 : cmd->Commands()) {
        if (!exec_cmd(cmd2, channels2, cmd->HasClient()))
            failed++;
    }

    // now we can set them all going at once
    std::vector<std::unique_lock<std::mutex>> locks;
    for (auto& st : stages2) {
        if (st->count_queued() == 1) {
            // just the waiter
            continue;
        }

        locks.push_back(st->get_lock());
    }

    // Now execute them all
    for (auto& st : stages2) {
        st->release();
    }

    // Now wait for them all to finish
    for (auto& st : stages2) {
        // TODO - move the locks to inside stage_delayed so that it can be released once the channel is done
        // TODO - when doing so, make sure that a swap locks on the delay executor too, to make sure it works
        st->wait();
    }
    locks.clear();

    // TODO report failed count?
    cmd->SendReply(L"202 COMMIT OK\r\n");

    CASPAR_LOG(warning) << "Executed command (" << timer.elapsed() << "s): " << cmd->name();
}

}}} // namespace caspar::protocol::amcp
