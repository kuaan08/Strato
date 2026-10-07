// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "render_resolution_state.h"
#include <gpu/memory_manager.h>
#include "resolution_scaling.h"
#include "resolution_budget.h"

namespace skyline::gpu {
    // The guest texture and its mappings remain native. This image is only an
    // attachment for eligible graphics passes, never a guest-visible texture.
    struct ScaledRenderTarget {
        std::shared_ptr<resolution::Reservation> reservation;
        memory::Image image;
        vk::raii::ImageView view;
        vk::Extent2D extent;
        bool initialized{};

        ScaledRenderTarget(std::shared_ptr<resolution::Reservation> reservation, memory::Image &&image, vk::raii::ImageView &&view, vk::Extent2D extent)
            : reservation{std::move(reservation)}, image{std::move(image)}, view{std::move(view)}, extent{extent} {}
    };

}
