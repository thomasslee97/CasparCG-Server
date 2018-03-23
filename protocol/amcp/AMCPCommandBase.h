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

#include <string>

namespace caspar { namespace protocol { namespace amcp {

class AMCPCommandBase
{
  public:
    virtual ~AMCPCommandBase() = default;

    virtual bool Execute()                                = 0; // TODO - is return type needed?
    virtual void SendReply(const std::wstring& str) const = 0; // TODO - really?

    virtual std::wstring name() const          = 0;
    virtual int          channel_index() const = 0; // TODO - can this be removed?
};

}}} // namespace caspar::protocol::amcp
