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
* Author: Robert Nagy, ronag89@gmail.com
*/

#include "../stdafx.h"

#if defined(_MSC_VER)
#pragma warning (disable : 4146)
#pragma warning (disable : 4244)
#endif

#include "flash_producer.h"
#include "FlashAxContainer.h"

#include "../util/swf.h"

#include <core/video_format.h>

#include <core/frame/frame.h>
#include <core/frame/draw_frame.h>
#include <core/frame/frame_factory.h>
#include <core/frame/pixel_format.h>
#include <core/monitor/monitor.h>

#include <common/env.h>
#include <common/executor.h>
#include <common/lock.h>
#include <common/diagnostics/graph.h>
#include <common/prec_timer.h>
#include <common/array.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>
#include <boost/timer.hpp>
#include <boost/algorithm/string.hpp>

#include <tbb/spin_mutex.h>

#include <asmlib.h>

#include <functional>

namespace caspar { namespace flash {
		
class bitmap
{
public:
	bitmap(int width, int height)
		: bmp_data_(nullptr)
		, hdc_(CreateCompatibleDC(0), DeleteDC)
	{	
		BITMAPINFO info;
		memset(&info, 0, sizeof(BITMAPINFO));
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		info.bmiHeader.biHeight = static_cast<LONG>(-height);
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biSize = sizeof(BITMAPINFO);
		info.bmiHeader.biWidth = static_cast<LONG>(width);

		bmp_.reset(CreateDIBSection(static_cast<HDC>(hdc_.get()), &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&bmp_data_), 0, 0), DeleteObject);
		SelectObject(static_cast<HDC>(hdc_.get()), bmp_.get());	

		if(!bmp_data_)
			CASPAR_THROW_EXCEPTION(bad_alloc());
	}

	operator HDC() {return static_cast<HDC>(hdc_.get());}

	BYTE* data() { return bmp_data_;}
	const BYTE* data() const { return bmp_data_;}

private:
	BYTE* bmp_data_;	
	std::shared_ptr<void> hdc_;
	std::shared_ptr<void> bmp_;
};

struct template_host
{
	std::wstring  video_mode;
	std::wstring  filename;
	int			  width;
	int			  height;
};

template_host get_template_host(const core::video_format_desc& desc)
{
	try
	{
		std::vector<template_host> template_hosts;
		BOOST_FOREACH(auto& xml_mapping, env::properties().get_child(L"configuration.template-hosts"))
		{
			try
			{
				template_host template_host;
				template_host.video_mode		= xml_mapping.second.get(L"video-mode", L"");
				template_host.filename			= xml_mapping.second.get(L"filename",	L"cg.fth");
				template_host.width				= xml_mapping.second.get(L"width",		desc.width);
				template_host.height			= xml_mapping.second.get(L"height",		desc.height);
				template_hosts.push_back(template_host);
			}
			catch(...){}
		}

		auto template_host_it = boost::find_if(template_hosts, [&](template_host template_host){return template_host.video_mode == desc.name;});
		if(template_host_it == template_hosts.end())
			template_host_it = boost::find_if(template_hosts, [&](template_host template_host){return template_host.video_mode == L"";});

		if(template_host_it != template_hosts.end())
			return *template_host_it;
	}
	catch(...){}
		
	template_host template_host;
	template_host.filename = L"cg.fth";

	for(auto it = boost::filesystem::directory_iterator(env::template_folder()); it != boost::filesystem::directory_iterator(); ++it)
	{
		if(boost::iequals(it->path().extension().wstring(), L"." + desc.name))
		{
			template_host.filename = it->path().filename().wstring();
			break;
		}
	}

	template_host.width =  desc.square_width;
	template_host.height = desc.square_height;
	return template_host;
}

class flash_renderer
{	
	struct com_init
	{
		HRESULT result_;

		com_init()
			: result_(CoInitialize(nullptr))
		{
			if(FAILED(result_))
				CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info("Failed to initialize com-context for flash-player"));
		}

		~com_init()
		{
			if(SUCCEEDED(result_))
				::CoUninitialize();
		}
	} com_init_;

	monitor::basic_subject&						event_subject_;

	const std::wstring							filename_;

	const std::shared_ptr<core::frame_factory>	frame_factory_;
	
	CComObject<caspar::flash::FlashAxContainer>* ax_;
	core::draw_frame							head_;
	bitmap										bmp_;
	
	spl::shared_ptr<diagnostics::graph>			graph_;
	
	prec_timer									timer_;

	const int									width_;
	const int									height_;
	
public:
	flash_renderer(monitor::basic_subject& event_subject, const spl::shared_ptr<diagnostics::graph>& graph, const std::shared_ptr<core::frame_factory>& frame_factory, const std::wstring& filename, int width, int height) 
		: event_subject_(event_subject)
		, graph_(graph)
		, filename_(filename)
		, frame_factory_(frame_factory)
		, ax_(nullptr)
		, head_(core::draw_frame::empty())
		, bmp_(width, height)
		, width_(width)
		, height_(height)
	{		
		graph_->set_color("frame-time", diagnostics::color(0.1f, 1.0f, 0.1f));
		graph_->set_color("param", diagnostics::color(1.0f, 0.5f, 0.0f));	
		graph_->set_color("sync", diagnostics::color(0.8f, 0.3f, 0.2f));			
		
		if(FAILED(CComObject<caspar::flash::FlashAxContainer>::CreateInstance(&ax_)))
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to create FlashAxContainer"));
		
		ax_->set_print([this]{return print();});

		if(FAILED(ax_->CreateAxControl()))
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to Create FlashAxControl"));
		
		CComPtr<IShockwaveFlash> spFlash;
		if(FAILED(ax_->QueryControl(&spFlash)))
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to Query FlashAxControl"));
												
		if(FAILED(spFlash->put_Playing(true)) )
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to start playing Flash"));

		if(FAILED(spFlash->put_Movie(CComBSTR(filename.c_str()))))
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to Load Template Host"));
										
		if(FAILED(spFlash->put_ScaleMode(2)))  //Exact fit. Scale without respect to the aspect ratio.
			CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(print() + L" Failed to Set Scale Mode"));
						
		ax_->SetSize(width_, height_);		

		tick(false);
		render();

		CASPAR_LOG(info) << print() << L" Initialized.";
	}

	~flash_renderer()
	{		
		if(ax_)
		{
			ax_->DestroyAxControl();
			ax_->Release();
		}
		graph_->set_value("tick-time", 0.0f);
		graph_->set_value("frame-time", 0.0f);
		CASPAR_LOG(info) << print() << L" Uninitialized.";
	}
	
	std::wstring call(const std::wstring& param)
	{		
		std::wstring result;

		CASPAR_LOG(trace) << print() << " Call: " << param;

		if(!ax_->FlashCall(param, result))
			CASPAR_LOG(warning) << print() << L" Flash call failed:" << param;//CASPAR_THROW_EXCEPTION(invalid_operation() << msg_info("Flash function call failed.") << arg_name_info("param") << arg_value_info(narrow(param)));
		graph_->set_tag("param");

		return result;
	}

	void tick(double sync)
	{		
		const float frame_time = 1.0f/ax_->GetFPS();

		if(sync > 0.00001)			
			timer_.tick(frame_time*sync); // This will block the thread.
		else
			graph_->set_tag("sync");

		graph_->set_value("sync", sync);
		event_subject_ << monitor::event("sync") % sync;
		
		ax_->Tick();
					
		MSG msg;
		while(PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) // DO NOT REMOVE THE MESSAGE DISPATCH LOOP. Without this some stuff doesn't work!  
		{
			if(msg.message == WM_TIMER && msg.wParam == 3 && msg.lParam == 0) // We tick this inside FlashAxContainer
				continue;
			
			TranslateMessage(&msg);
			DispatchMessage(&msg);			
		}
	}
	
	core::draw_frame render()
	{			
		const float frame_time = 1.0f/fps();

		boost::timer frame_timer;

		if(ax_->InvalidRect())
		{			
			A_memset(bmp_.data(), 0, width_*height_*4);
			ax_->DrawControl(bmp_);
		
			core::pixel_format_desc desc = core::pixel_format::bgra;
			desc.planes.push_back(core::pixel_format_desc::plane(width_, height_, 4));
			auto frame = frame_factory_->create_frame(this, desc);

			A_memcpy(frame.image_data(0).begin(), bmp_.data(), width_*height_*4);
			head_ = core::draw_frame(std::move(frame));	
		}		
										
		graph_->set_value("frame-time", static_cast<float>(frame_timer.elapsed()/frame_time)*0.5f);
		event_subject_ << monitor::event("renderer/profiler/time") % frame_timer.elapsed() % frame_time;
		return head_;
	}
	
	bool is_empty() const
	{
		return ax_->IsEmpty();
	}

	double fps() const
	{
		return ax_->GetFPS();	
	}
	
	std::wstring print()
	{
		return L"flash-player[" + boost::filesystem::wpath(filename_).filename().wstring() 
				  + L"|" + boost::lexical_cast<std::wstring>(width_)
				  + L"x" + boost::lexical_cast<std::wstring>(height_)
				  + L"]";		
	}
};

struct flash_producer : public core::frame_producer_base
{	
	monitor::basic_subject							event_subject_;
	const std::wstring								filename_;	
	const spl::shared_ptr<core::frame_factory>		frame_factory_;
	const core::video_format_desc					format_desc_;
	const int										width_;
	const int										height_;
	core::constraints								constraints_;
	const int										buffer_size_;

	tbb::atomic<int>								fps_;

	spl::shared_ptr<diagnostics::graph>				graph_;

	std::queue<core::draw_frame>					frame_buffer_;
	tbb::concurrent_bounded_queue<core::draw_frame>	output_buffer_;

	core::draw_frame								last_frame_;
				
	boost::timer									tick_timer_;
	std::unique_ptr<flash_renderer>					renderer_;
	
	executor										executor_;	
public:
	flash_producer(const spl::shared_ptr<core::frame_factory>& frame_factory, const core::video_format_desc& format_desc, const std::wstring& filename, int width, int height) 
		: filename_(filename)		
		, frame_factory_(frame_factory)
		, format_desc_(format_desc)
		, width_(width > 0 ? width : format_desc.width)
		, height_(height > 0 ? height : format_desc.height)
		, constraints_(width_, height_)
		, buffer_size_(env::properties().get(L"configuration.flash.buffer-depth", format_desc.fps > 30.0 ? 4 : 2))
		, executor_(L"flash_producer")
	{	
		fps_ = 0;
	 
		graph_->set_color("buffer-size", diagnostics::color(1.0f, 1.0f, 0.0f));
		graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));
		graph_->set_color("late-frame", diagnostics::color(0.6f, 0.3f, 0.9f));
		graph_->set_text(print());
		diagnostics::register_graph(graph_);

		CASPAR_LOG(info) << print() << L" Initialized";
	}

	~flash_producer()
	{
		executor_.invoke([this]
		{
			renderer_.reset();
		}, task_priority::high_priority);
	}

	// frame_producer
		
	core::draw_frame receive_impl() override
	{					
		auto frame = last_frame_;
		
		if(output_buffer_.try_pop(frame))			
			executor_.begin_invoke(std::bind(&flash_producer::next, this));		
		else		
			graph_->set_tag("late-frame");		
				
		event_subject_ << monitor::event("host/path")	% filename_
					   << monitor::event("host/width")	% width_
					   << monitor::event("host/height") % height_
					   << monitor::event("host/fps")	% fps_
					   << monitor::event("buffer")		% output_buffer_.size() % buffer_size_;

		return last_frame_ = frame;
	}

	core::constraints& pixel_constraints() override
	{
		return constraints_;
	}
		
	boost::unique_future<std::wstring> call(const std::vector<std::wstring>& params) override
	{
		auto param = boost::algorithm::join(params, L" ");

		return executor_.begin_invoke([this, param]() -> std::wstring
		{			
			try
			{
				if(!renderer_)
				{
					renderer_.reset(new flash_renderer(event_subject_, graph_, frame_factory_, filename_, width_, height_));

					while(output_buffer_.size() < buffer_size_)
						output_buffer_.push(core::draw_frame::empty());
				}

				return renderer_->call(param);	
			}
			catch(...)
			{
				CASPAR_LOG_CURRENT_EXCEPTION();
				renderer_.reset(nullptr);
			}

			return L"";
		});
	}
		
	std::wstring print() const override
	{ 
		return L"flash[" + boost::filesystem::path(filename_).wstring() + L"|" + boost::lexical_cast<std::wstring>(fps_) + L"]";		
	}	

	std::wstring name() const override
	{
		return L"flash";
	}

	boost::property_tree::wptree info() const override
	{
		boost::property_tree::wptree info;
		info.add(L"type", L"flash");
		return info;
	}

	void subscribe(const monitor::observable::observer_ptr& o) override
	{
		event_subject_.subscribe(o);
	}

	void unsubscribe(const monitor::observable::observer_ptr& o) override
	{
		event_subject_.unsubscribe(o);
	}

	// flash_producer
	
	void tick()
	{
		double ratio = std::min(1.0, static_cast<double>(output_buffer_.size())/static_cast<double>(std::max(1, buffer_size_ - 1)));
		double sync  = 2*ratio - ratio*ratio;
		renderer_->tick(sync);
	}
	
	void next()
	{	
		if(!renderer_)
			frame_buffer_.push(core::draw_frame::empty());

		tick_timer_.restart();				

		if(frame_buffer_.empty())
		{					
			tick();
			auto frame = renderer_->render();

			if(abs(renderer_->fps()/2.0 - format_desc_.fps) < 2.0) // flash == 2 * format -> interlace
			{					
				tick();
				if(format_desc_.field_mode != core::field_mode::progressive)
					frame = core::draw_frame::interlace(frame, renderer_->render(), format_desc_.field_mode);
				
				frame_buffer_.push(frame);
			}
			else if(abs(renderer_->fps() - format_desc_.fps/2.0) < 2.0) // format == 2 * flash -> duplicate
			{
				frame_buffer_.push(frame);
				frame_buffer_.push(frame);
			}
			else //if(abs(renderer_->fps() - format_desc_.fps) < 0.1) // format == flash -> simple
			{
				frame_buffer_.push(frame);
			}
						
			fps_.fetch_and_store(static_cast<int>(renderer_->fps()*100.0));				
			graph_->set_text(print());
			
			if(renderer_->is_empty())			
				renderer_.reset();
		}

		graph_->set_value("tick-time", static_cast<float>(tick_timer_.elapsed()/fps_)*0.5f);
		event_subject_ << monitor::event("profiler/time") % tick_timer_.elapsed() % fps_;

		output_buffer_.push(std::move(frame_buffer_.front()));
		frame_buffer_.pop();
	}
};

spl::shared_ptr<core::frame_producer> create_producer(const spl::shared_ptr<core::frame_factory>& frame_factory, const core::video_format_desc& format_desc, const std::vector<std::wstring>& params)
{
	auto template_host = get_template_host(format_desc);
	
	auto filename = env::template_folder() + L"\\" + template_host.filename;
	
	if(!boost::filesystem::exists(filename))
		CASPAR_THROW_EXCEPTION(file_not_found() << boost::errinfo_file_name(u8(filename)));	

	return create_destroy_proxy(spl::make_shared<flash_producer>(frame_factory, format_desc, filename, template_host.width, template_host.height));
}

spl::shared_ptr<core::frame_producer> create_swf_producer(const spl::shared_ptr<core::frame_factory>& frame_factory, const core::video_format_desc& format_desc, const std::vector<std::wstring>& params)
{
	auto filename = env::media_folder() + L"\\" + params.at(0) + L".swf";

	if (!boost::filesystem::exists(filename))
		return core::frame_producer::empty();

	swf_t::header_t header(filename);

	return create_destroy_proxy(
		spl::make_shared<flash_producer>(frame_factory, format_desc, filename, header.frame_width, header.frame_height));
}

std::wstring find_template(const std::wstring& template_name)
{
	if(boost::filesystem::exists(template_name + L".ft")) 
		return template_name + L".ft";
	
	if(boost::filesystem::exists(template_name + L".ct"))
		return template_name + L".ct";
	
	if(boost::filesystem::exists(template_name + L".swf"))
		return template_name + L".swf";

	return L"";
}

}}