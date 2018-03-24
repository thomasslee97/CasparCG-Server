#pragma once

#include "../util/lock_container.h"
#include <common/memory.h>
#include <core/video_channel.h>

namespace caspar { namespace protocol { namespace amcp {

class channel_context
{
  public:
    explicit channel_context() {}
    explicit channel_context(const std::shared_ptr<core::video_channel>& c, const std::wstring& lifecycle_key)
        : channel(c)
        , lock(std::make_shared<caspar::IO::lock_container>(lifecycle_key))
    {
    }
    std::shared_ptr<core::video_channel>        channel;
    std::shared_ptr<caspar::IO::lock_container> lock;
};

struct command_context_simple
{
    const IO::ClientInfoPtr   client;
    const int                 channel_index;
    const int                 layer_id;
    const std::vector<std::wstring> parameters;

    int layer_index(int default_ = 0) const { return layer_id == -1 ? default_ : layer_id; }

    command_context_simple(IO::ClientInfoPtr client, int channel_index, int layer_id, const std::vector<std::wstring> parameters)
        : client(std::move(client))
        , channel_index(channel_index)
        , layer_id(layer_id)
        , parameters(parameters)
    {
    }
};

typedef std::function<std::wstring(const command_context_simple& args)> amcp_command_func;

}}} // namespace caspar::protocol::amcp