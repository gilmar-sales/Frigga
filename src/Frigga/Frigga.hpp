#pragma once

#include <Freya/Vulkan.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include "Frigga/Animation/AnimationController.hpp"
#include "Frigga/Core/AbstractApplication.hpp"
#include "Frigga/Net/Network.hpp"

namespace FRIGGA_NAMESPACE
{

    class FriggaExtension final: public skr::IExtension
    {
      public:
        FriggaExtension &SetHeadless(bool headless)
        {
            mHeadless = headless;
            return *this;
        }

        [[nodiscard]] bool IsHeadless() const
        {
            return mHeadless;
        }

        void Attach(skr::ApplicationBuilder &applicationBuilder) override;
        void ConfigureServices(skr::ServiceCollection &services) override;

      private:
        bool mHeadless = false;
    };

} // namespace FRIGGA_NAMESPACE
