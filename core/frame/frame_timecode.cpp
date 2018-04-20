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
#include "frame_timecode.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/format.hpp>
#include <common/future.h>

namespace caspar { namespace core {

frame_timecode validate(const frame_timecode& timecode, int delta)
{
    const uint8_t fps = 25; // TODO dynamic

    int frames        = timecode.frames() + delta;
    int delta_seconds = frames / fps;
    frames %= fps;
    if (frames < 0) {
        frames += fps;
        delta_seconds -= 1;
    }

    int seconds       = timecode.seconds() + delta_seconds;
    int delta_minutes = seconds / 60;
    seconds %= 60;
    if (seconds < 0) {
        seconds += 60;
        delta_minutes -= 1;
    }

    int minutes     = timecode.minutes() + delta_minutes;
    int delta_hours = minutes / 60;
    minutes %= 60;
    if (minutes < 0) {
        minutes += 60;
        delta_hours -= 1;
    }

    int hours = timecode.hours() + delta_hours;
    hours %= 24;
    if (hours < 0) {
        hours += 24;
    }

    return frame_timecode(static_cast<uint8_t>(hours),
                          static_cast<uint8_t>(minutes),
                          static_cast<uint8_t>(seconds),
                          static_cast<uint8_t>(frames),
                          fps);
}

const std::wstring frame_timecode::string() const
{
    return (boost::wformat(L"%02i:%02i:%02i:%02i") % hours_ % minutes_ % seconds_ % frames_)
        .str(); // TODO frames format
}

unsigned int frame_timecode::bcd() const
{
    unsigned int res = 0;

    res += ((hours_ / 10) << 4) + (hours_ % 10);
    res <<= 8;
    res += ((minutes_ / 10) << 4) + (minutes_ % 10);
    res <<= 8;
    res += ((seconds_ / 10) << 4) + (seconds_ % 10);
    res <<= 8;
    const uint8_t fr = frames_small();
    res += ((fr / 10) << 4) + (fr % 10);

    return res;
}

uint32_t frame_timecode::total_frames() const
{
    uint32_t res = 0;

    res += hours_;
    res *= 60;

    res += minutes_;
    res *= 60;

    res += seconds_;
    res *= fps_;

    res += frames_;

    return res;
}

uint32_t frame_timecode::max_frames() const
{
    static uint32_t num_seconds = 24 * 60 * 60;

    return num_seconds * fps_ - 1;
}

int64_t frame_timecode::pts() const
{
    int64_t res = total_frames();
    res *= 1000;
    res /= fps_;

    return res;
}

frame_timecode::frame_timecode()
    : hours_(0)
    , minutes_(0)
    , seconds_(0)
    , frames_(0)
    , fps_(0)
{
}

frame_timecode::frame_timecode(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames, uint8_t fps)
    : hours_(hours)
    , minutes_(minutes)
    , seconds_(seconds)
    , frames_(frames)
    , fps_(fps)
{
}

frame_timecode::frame_timecode(uint32_t frames, uint8_t fps)
    : hours_(0)
    , minutes_(0)
    , seconds_(0)
    , frames_(0)
    , fps_(fps)
{
    *this = validate(*this, frames); // TODO - does this work?
}

const frame_timecode& frame_timecode::get_default()
{
    static const frame_timecode def = {0, 0, 0, 0, 0};

    return def;
}

bool frame_timecode::parse_string(const std::wstring& str, frame_timecode& res)
{
    if (str.length() != 11)
        return false;

    std::vector<std::wstring> strs;
    boost::split(strs, str, boost::is_any_of(":.;,"));

    if (strs.size() != 4)
        return false;

    try {
        const uint8_t hours   = static_cast<uint8_t>(std::stoi(strs[0]));
        const uint8_t minutes = static_cast<uint8_t>(std::stoi(strs[1]));
        const uint8_t seconds = static_cast<uint8_t>(std::stoi(strs[2]));
        const uint8_t frames  = static_cast<uint8_t>(std::stoi(strs[3]));

        // TODO fps
        res = core::frame_timecode(hours, minutes, seconds, frames, 25);
        return true;
    } catch (...) {
        return false;
    }
}

// const frame_timecode& frame_timecode::from_int(uint64_t frame_count, const struct pixel_format_desc* format)
//{
//    return frame_timecode::get_default();
//}

bool frame_timecode::operator<(const frame_timecode& other) const
{
    if (hours_ != other.hours_)
        return hours_ < other.hours_;

    if (minutes_ != other.minutes_)
        return minutes_ < other.minutes_;

    if (seconds_ != other.seconds_)
        return seconds_ < other.seconds_;

    if (frames_ != other.frames_)
        return frames_ < other.frames_;

    // TODO - account for framerate

    return false;
}

bool frame_timecode::operator>(const frame_timecode& other) const
{
    if (hours_ != other.hours_)
        return hours_ > other.hours_;

    if (minutes_ != other.minutes_)
        return minutes_ > other.minutes_;

    if (seconds_ != other.seconds_)
        return seconds_ > other.seconds_;

    if (frames_ != other.frames_)
        return frames_ > other.frames_;

    // TODO - account for framerate

    return false;
}

bool frame_timecode::operator<=(const frame_timecode& other) const { return !(*this > other); }
bool frame_timecode::operator>=(const frame_timecode& other) const { return !(*this < other); }

bool frame_timecode::operator==(const frame_timecode& other) const
{
    if (hours_ != other.hours_)
        return false;

    if (minutes_ != other.minutes_)
        return false;

    if (seconds_ != other.seconds_)
        return false;

    if (frames_ != other.frames_)
        return false;

    // TODO - account for framerate

    return true;
}

bool frame_timecode::operator!=(const frame_timecode& other) const { return !((*this) == other); }

frame_timecode frame_timecode::operator+=(int delta) { return *this = validate(*this, delta); }
frame_timecode frame_timecode::operator-=(int delta) { return *this = validate(*this, -delta); }
frame_timecode frame_timecode::operator+(int delta) const { return validate(*this, delta); }
frame_timecode frame_timecode::operator-(int delta) const { return validate(*this, -delta); }

}} // namespace caspar::core
