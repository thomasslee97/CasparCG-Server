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

#include <cstdint>
#include <vector>

namespace caspar { namespace core {

// TODO - move timecode bits to seperate file

// TODO - make immutable
struct frame_timecode
{
    frame_timecode();
    frame_timecode(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames, uint8_t fps);

    uint8_t hours() const { return hours_; }
    uint8_t minutes() const { return minutes_; }
    uint8_t seconds() const { return seconds_; }
    uint8_t frames() const { return frames_; }
    uint8_t fps() const { return fps_; }

    bool is_valid() const { return fps_ != 0; }

    static const frame_timecode& get_default();
    static bool                  parse_string(const std::wstring& str, frame_timecode& res);

    //    static const frame_timecode& from_int(uint64_t frame_count, const struct pixel_format_desc* format);

    bool operator<(const frame_timecode& other) const;
    bool operator>(const frame_timecode& other) const;
    bool operator<=(const frame_timecode& other) const;
    bool operator>=(const frame_timecode& other) const;

    bool operator==(const frame_timecode& other) const;
    bool operator!=(const frame_timecode& other) const;

    frame_timecode operator+=(int frames);
    frame_timecode operator-=(int frames);
    frame_timecode operator+(int frames) const;
    frame_timecode operator-(int frames) const;

    const std::wstring string() const;
    unsigned int       bcd() const;

  private:
    uint8_t hours_;
    uint8_t minutes_;
    uint8_t seconds_;
    uint8_t frames_;
    uint8_t fps_;
};

// this is mutable, updated by decklink_producer and can return frame_timecode for use by consumers
class channel_timecode
{
  public:
    explicit channel_timecode()
        : timecode_(frame_timecode::get_default())
    {
    }

    void           tick();
    frame_timecode timecode() const { return timecode_; };

  private:
    frame_timecode timecode_;
};
}} // namespace caspar::core
