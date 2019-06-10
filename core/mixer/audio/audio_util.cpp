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
* Author: Julian Waller, julian@superfly.tv
*/

#include <boost/lexical_cast.hpp>

#include "audio_util.h"

namespace caspar { namespace core {

std::vector<int32_t> audio_max_level_for_frame(const int num_channels, const int32_t* result, const size_t size) {
	auto max = std::vector<int32_t>(num_channels, std::numeric_limits<int32_t>::min());

	for (size_t n = 0; n < size; n += num_channels)
		for (int ch = 0; ch < num_channels; ++ch)
			max[ch] = std::max(max[ch], std::abs(result[n + ch]));

	return max;
}

void output_audio_levels(monitor::subject& monitor_subject, const std::vector<int32_t> max_values) {

	// Makes the dBFS of silence => -dynamic range of 32bit LPCM => about -192 dBFS
	// Otherwise it would be -infinity
	static const auto MIN_PFS = 0.5f / static_cast<float>(std::numeric_limits<int32_t>::max());

	for (int i = 0; i < max_values.size(); ++i)
	{
		const auto pFS = max_values[i] / static_cast<float>(std::numeric_limits<int32_t>::max());
		const auto dBFS = 20.0f * std::log10(std::max(MIN_PFS, pFS));

		auto chan_str = boost::lexical_cast<std::string>(i + 1);

		monitor_subject << monitor::message("/" + chan_str + "/pFS") % pFS;
		monitor_subject << monitor::message("/" + chan_str + "/dBFS") % dBFS;
	}
}

}}