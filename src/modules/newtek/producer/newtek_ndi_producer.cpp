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

#include "newtek_ndi_producer.h"

#include <common/assert.h>

#include "../util/ndi_instance.h"


namespace caspar { namespace newtek {

struct newtek_ndi_producer : public core::frame_producer_base {
    core::monitor::state state_;
    const std::wstring description_;
    const spl::shared_ptr <core::frame_factory> frame_factory_;
    const uint32_t length_;
    core::draw_frame frame_;

    std::shared_ptr<const NDIlib_v3> ndi_instance_;
    NDIlib_recv_instance_t recv_instance_;

    newtek_ndi_producer(const spl::shared_ptr <core::frame_factory> &frame_factory,
                        const std::wstring &description,
                        uint32_t length)
            : description_(description), frame_factory_(frame_factory), length_(length) {
        ndi_instance_ = ndi::get_instance();
        if (!ndi_instance_) {
            CASPAR_THROW_EXCEPTION(not_supported() << msg_info(L"Newtek NDI not available"));
        }

        NDIlib_source_t source;
        //source.p_ip_address = "";
        //source.p_ndi_name = "";

        NDIlib_recv_create_v3_t settings;
        settings.allow_video_fields = false;
        // settings.bandwidth // TODO an option
        settings.color_format = NDIlib_recv_color_format_e_BGRX_BGRA;
        settings.source_to_connect_to = source;

        recv_instance_ = ndi_instance_->NDIlib_recv_create_v3(&settings);
        CASPAR_VERIFY(recv_instance_);

        CASPAR_LOG(info) << print() << L" Initialized";
    }

    // frame_producer

    core::draw_frame receive_impl(int nb_samples) override {
        state_["file/path"] = description_;

        return frame_;
    }

    uint32_t nb_frames() const override { return length_; }

    std::wstring print() const override { return L"newtek_ndi_producer[" + description_ + L"]"; }

    std::wstring name() const override { return L"newtek_ndi"; }

    const core::monitor::state &state() const { return state_; }
};

}}
