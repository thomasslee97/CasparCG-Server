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

#include <memory>
#include "video_format.h"
#include "frame/frame_timecode.h"
#include "producer/timecode_source.h"

namespace caspar { namespace core {

class channel_timecode
{
  public:
    explicit channel_timecode(int index, const video_format_desc& format);

    void start();

    frame_timecode tick();

    frame_timecode timecode() const;
    void           timecode(frame_timecode& tc);

    void change_format(const video_format_desc& format);

    bool is_free() const;
    std::wstring source_name() const;

    void set_source(std::shared_ptr<core::timecode_source> src);
    void set_weak_source(std::shared_ptr<core::timecode_source> src);
    void clear_source();
    void set_system_time();

  private:
    struct impl;
    std::shared_ptr<impl> impl_;
};

}}
