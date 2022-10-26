#pragma once

#include <glm/glm.hpp>

FRIGGA_BEGIN

struct NameComponent
{
    std::string name;

    NameComponent() = default;
    NameComponent(const NameComponent&) = default;
    NameComponent(const std::string& name)
        : name(name) {}
};

FRIGGA_END
