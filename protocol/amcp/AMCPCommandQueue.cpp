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

#include <boost/lexical_cast.hpp>

namespace caspar { namespace protocol { namespace amcp {

AMCPCommandQueue::AMCPCommandQueue(const std::wstring& name, const std::vector<channel_context>& channels)
    : executor_(L"AMCPCommandQueue " + name)
    , channels_(channels)
{
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
            pCurrentCommand->Execute(channels_);

            CASPAR_LOG(trace) << "Ready for a new command";
        } catch (...) {
            CASPAR_LOG_CURRENT_EXCEPTION();
        }
    });
}

}}} // namespace caspar::protocol::amcp
