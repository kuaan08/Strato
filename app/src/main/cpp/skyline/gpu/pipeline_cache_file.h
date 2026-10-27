// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>

namespace skyline::gpu {
    /**
     * @brief Header that precedes serialized Vulkan pipeline cache data.
     */
    struct PipelineCacheFileDataHeader {
        std::uint64_t size;
        std::uint64_t hash;
    };
    static_assert(sizeof(PipelineCacheFileDataHeader) == 0x10);

    /**
     * @brief Reject truncated, inconsistent, or unrepresentable cache payload sizes.
     *
     * This check must run before using the declared size for allocation or stream reads.
     */
    inline constexpr bool IsPipelineCachePayloadSizeValid(std::uintmax_t fileSize, std::uint64_t payloadSize) noexcept {
        constexpr std::uintmax_t headerSize{sizeof(PipelineCacheFileDataHeader)};
        if (fileSize < headerSize || payloadSize != fileSize - headerSize)
            return false;

        return payloadSize <= std::numeric_limits<std::size_t>::max() &&
               payloadSize <= static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max());
    }
}
