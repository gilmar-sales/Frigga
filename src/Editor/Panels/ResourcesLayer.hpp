#pragma once

#include <Frigga/Core/Layer.hpp>

class ResourcesLayer: public fg::Layer
{
public:
    ResourcesLayer(): fg::Layer("Resources") {}
    ~ResourcesLayer() = default;

    void onGui() override;

    static bool IsOpen;

private:
};