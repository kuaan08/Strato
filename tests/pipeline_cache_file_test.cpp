#include <cassert>
#include <cstdint>
#include <limits>
#include "skyline/gpu/pipeline_cache_file.h"

using skyline::gpu::IsPipelineCachePayloadSizeValid;
using skyline::gpu::PipelineCacheFileDataHeader;

int main() {
    constexpr std::uint64_t headerSize{sizeof(PipelineCacheFileDataHeader)};
    static_assert(headerSize == 16);

    assert(IsPipelineCachePayloadSizeValid(headerSize, 0));
    assert(IsPipelineCachePayloadSizeValid(headerSize + 1, 1));
    assert(IsPipelineCachePayloadSizeValid(headerSize + 4096, 4096));

    assert(!IsPipelineCachePayloadSizeValid(0, 0));
    assert(!IsPipelineCachePayloadSizeValid(headerSize - 1, 0));
    assert(!IsPipelineCachePayloadSizeValid(headerSize + 1, 0));
    assert(!IsPipelineCachePayloadSizeValid(headerSize + 16, 15));
    assert(!IsPipelineCachePayloadSizeValid(headerSize + 16, 17));

    constexpr auto largestFile{std::numeric_limits<std::uint64_t>::max()};
    assert(!IsPipelineCachePayloadSizeValid(largestFile, largestFile - headerSize));
}
