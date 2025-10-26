#pragma once

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include "Frigga/Core/AbstractApplication.hpp"

namespace FRIGGA_NAMESPACE
{

    class FriggaExtension final: public skr::IExtension
    {
        void Attach(skr::ApplicationBuilder &applicationBuilder) override;
        void ConfigureServices(skr::ServiceCollection &services) override;
    };

} // namespace FRIGGA_NAMESPACE