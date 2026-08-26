#pragma once

#include "Editor/ViewportTarget.hpp"

#include <cstdint>
#include <vector>

namespace EditorViewportHost
{
    struct ClaimRequest
    {
        fg::ViewportTarget *target = nullptr;
        std::uint32_t       width  = 0;
        std::uint32_t       height = 0;
        bool                active = false;
    };

    inline std::vector<ClaimRequest> &Requests()
    {
        static std::vector<ClaimRequest> requests;
        return requests;
    }

    inline void BeginFrame()
    {
        Requests().clear();
    }

    inline void Request(ClaimRequest request)
    {
        if(request.target != nullptr)
        {
            Requests().push_back(request);
        }
    }

    inline void ApplyClaims()
    {
        fg::ViewportTarget *winner = nullptr;
        std::uint32_t       winW   = 0;
        std::uint32_t       winH   = 0;

        for(const ClaimRequest &req : Requests())
        {
            if(req.active && req.width > 0 && req.height > 0)
            {
                winner = req.target;
                winW   = req.width;
                winH   = req.height;
            }
        }

        for(const ClaimRequest &req : Requests())
        {
            if(req.target == nullptr)
            {
                continue;
            }

            if(req.target == winner)
            {
                req.target->Claim(winW, winH);
            }
            else
            {
                req.target->Suspend();
            }
        }
    }

    [[nodiscard]] inline bool HasActiveClaim()
    {
        for(const ClaimRequest &req : Requests())
        {
            if(req.active && req.width > 0 && req.height > 0)
            {
                return true;
            }
        }
        return false;
    }
} // namespace EditorViewportHost
