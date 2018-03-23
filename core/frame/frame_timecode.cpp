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

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/format.hpp>
#include <common/future.h>

#include <sstream>

namespace caspar { namespace core {

void channel_timecode::tick()
{
    timecode_.frames++;
    if (timecode_.frames >= 50) // TODO - account for framerate & drop frame
    {
        timecode_.frames = 0;
        timecode_.seconds++;
    }
    if (timecode_.seconds >= 60) {
        timecode_.seconds = 0;
        timecode_.minutes++;
    }
    if (timecode_.minutes >= 60) {
        timecode_.minutes = 0;
        timecode_.hours++;
    }
    if (timecode_.hours >= 24) {
        timecode_.hours = 0;
    }
}

const std::wstring frame_timecode::string() const
{
    return (boost::wformat(L"%02i:%02i:%02i:%02i") % hours % minutes % seconds % frames).str();
}

unsigned int frame_timecode::bcd() const
{
    unsigned int res = 0;

    res += ((hours / 10) << 4) + (hours % 10);
    res <<= 8;
    res += ((minutes / 10) << 4) + (minutes % 10);
    res <<= 8;
    res += ((seconds / 10) << 4) + (seconds % 10);
    res <<= 8;
    res += ((frames / 10) << 4) + (frames % 10);

    return res;
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
        res         = core::frame_timecode();
        res.hours   = static_cast<uint8_t>(std::stoi(strs[0]));
        res.minutes = static_cast<uint8_t>(std::stoi(strs[1]));
        res.seconds = static_cast<uint8_t>(std::stoi(strs[2]));
        res.frames  = static_cast<uint8_t>(std::stoi(strs[3]));
        // TODO fps?
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
    if (hours != other.hours)
        return hours < other.hours;

    if (minutes != other.minutes)
        return minutes < other.minutes;

    if (seconds != other.seconds)
        return seconds < other.seconds;

    if (frames != other.frames)
        return frames < other.frames;

    // TODO - account for framerate

    return false;
}

bool frame_timecode::operator==(const frame_timecode& other) const
{
    if (hours != other.hours)
        return false;

    if (minutes != other.minutes)
        return false;

    if (seconds != other.seconds)
        return false;

    if (frames != other.frames)
        return false;

    // TODO - account for framerate

    return true;
}

bool frame_timecode::operator!=(const frame_timecode& other) const { return !((*this) == other); }

// void frame_timecode::operator++() {
//    frames++;
//    if (frames >= 50)
//    {
//        frames = 0;
//        seconds++;
//    }
//    if (seconds >= 60)
//    {
//        seconds = 0;
//        minutes++;
//    }
//    if (minutes >= 60)
//    {
//        minutes = 0;
//        hours++;
//    }
//    if (hours >= 24)
//    {
//        hours = 0;
//    }
//}

}} // namespace caspar::core
