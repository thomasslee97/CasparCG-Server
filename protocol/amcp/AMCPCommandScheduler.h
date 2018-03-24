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

#include <core/frame/frame_timecode.h>

namespace caspar { namespace protocol { namespace amcp {

class AMCPCommandScheduler
{
  public:
    AMCPCommandScheduler();

    void add_channel(std::shared_ptr<core::channel_timecode> channel_timecode);

    void set(int                          channel_index,
             const std::wstring&          token,
             const core::frame_timecode&  timecode,
             std::shared_ptr<AMCPCommand> command);

    bool remove(const std::wstring& token);

    void clear();

    std::vector<std::pair<core::frame_timecode, std::wstring>> list(core::frame_timecode& timecode);

    boost::optional<std::vector<std::shared_ptr<AMCPGroupCommand>>> schedule(int channel_index);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}}} // namespace caspar::protocol::amcp
