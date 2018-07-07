/*
 * Copyright 2013 NewTek
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
 * Author: Julian Waller, git@julusian.co.uk
 */

#include "../StdAfx.h"

#include "newtek_ndi_consumer.h"

#include <core/consumer/frame_consumer.h>
#include <core/frame/frame.h>
#include <core/mixer/audio/audio_util.h>
#include <core/monitor/monitor.h>
#include <core/video_format.h>

#include <common/assert.h>
#include <common/diagnostics/graph.h>
#include <common/future.h>
#include <common/param.h>
#include <common/timer.h>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include <atomic>

#include "newtek/util/ndi_instance.h"

namespace caspar { namespace newtek {

struct newtek_ndi_consumer : public core::frame_consumer
{
    core::video_format_desc             format_desc_;
    core::monitor::state                state_;
    std::atomic<bool>                   connected_ = {false};
    spl::shared_ptr<diagnostics::graph> graph_;
    timer                               tick_timer_;

    std::shared_ptr<const NDIlib_v3> ndi_instance_;
    NDIlib_send_instance_t send_instance_;

  public:
    newtek_ndi_consumer()
    {
        ndi_instance_ = ndi::get_instance();
        if (!ndi_instance_) {
            CASPAR_THROW_EXCEPTION(not_supported() << msg_info(L"Newtek NDI not available"));
        }

        graph_->set_text(print());
        graph_->set_color("frame-time", diagnostics::color(0.5f, 1.0f, 0.2f));
        graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));
        graph_->set_color("dropped-frame", diagnostics::color(0.3f, 0.6f, 0.3f));
        diagnostics::register_graph(graph_);
    }

    ~newtek_ndi_consumer() {
        if (send_instance_) {
            ndi::get_instance()->NDIlib_send_destroy(send_instance_);
        }
    }

    // frame_consumer

    void initialize(const core::video_format_desc& format_desc, int channel_index) override
    {
        format_desc_ = format_desc;

        std::string name = "CasparCG " + boost::lexical_cast<std::string>(channel_index);
        // Create an NDI source that is clocked to the video.
        NDIlib_send_create_t NDI_send_create_desc;
        NDI_send_create_desc.p_ndi_name = name.c_str();
        NDI_send_create_desc.clock_video = true;

        send_instance_ = ndi::get_instance()->NDIlib_send_create(&NDI_send_create_desc);
        CASPAR_VERIFY(send_instance_);
    }

    std::future<bool> send(core::const_frame frame) override
    {
        CASPAR_VERIFY(format_desc_.height * format_desc_.width * 4 == frame.image_data(0).size());

        graph_->set_value("tick-time", tick_timer_.elapsed() * format_desc_.fps * 0.5);
        tick_timer_.restart();

        caspar::timer frame_timer;

        {
            auto audio_buffer = core::audio_32_to_float(frame.audio_data());

            NDIlib_audio_frame_v2_t audio_frame;
            audio_frame.sample_rate = format_desc_.audio_sample_rate;
            audio_frame.no_channels = format_desc_.audio_channels;
            audio_frame.no_samples = format_desc_.audio_cadence[0]; // TODO - rotate
            audio_frame.p_data = &audio_buffer[0];
            audio_frame.channel_stride_in_bytes = audio_frame.no_samples / audio_frame.no_channels;

            // TODO - is there a better send that allows us to avoid the conversions?
            ndi_instance_->NDIlib_send_send_audio_v2(send_instance_, &audio_frame);
        }

        {
            NDIlib_video_frame_v2_t ndi_frame;
            ndi_frame.FourCC = NDIlib_FourCC_type_BGRA;
            ndi_frame.xres = format_desc_.width;
            ndi_frame.yres = format_desc_.height;
            ndi_frame.line_stride_in_bytes = format_desc_.width * 4;
            ndi_frame.frame_rate_N = format_desc_.time_scale;
            ndi_frame.frame_rate_D = format_desc_.duration;
            ndi_frame.frame_format_type = NDIlib_frame_format_type_progressive; // TODO convert to interlaced
            ndi_frame.p_data = (uint8_t*)frame.image_data(0).data();

            ndi_instance_->NDIlib_send_send_video_v2(send_instance_, &ndi_frame);
        }

        connected_ = ndi_instance_->NDIlib_send_get_no_connections(send_instance_, 0);

        graph_->set_text(print());
        graph_->set_value("frame-time", frame_timer.elapsed() * format_desc_.fps * 0.5);

        return make_ready_future(true);
    }

    const core::monitor::state& state() const override { return state_; }

    std::wstring print() const override
    {
        return connected_ ? L"newtek-ndi[connected]" : L"newtek-ndi[not connected]";
    }

    std::wstring name() const override { return L"newtek-ndi"; }

    int index() const override { return 900; }

    bool has_synchronization_clock() const override { return false; }
};

spl::shared_ptr<core::frame_consumer> create_ndi_consumer(const std::vector<std::wstring>&                  params,
                                                           std::vector<spl::shared_ptr<core::video_channel>> channels)
{
    if (params.size() < 1 || !boost::iequals(params.at(0), L"NEWTEK_NDI"))
        return core::frame_consumer::empty();

    return spl::make_shared<newtek_ndi_consumer>();
}

spl::shared_ptr<core::frame_consumer>
create_preconfigured_ndi_consumer(const boost::property_tree::wptree&               ptree,
                                   std::vector<spl::shared_ptr<core::video_channel>> channels)
{
    return spl::make_shared<newtek_ndi_consumer>();
}

}} // namespace caspar::newtek
