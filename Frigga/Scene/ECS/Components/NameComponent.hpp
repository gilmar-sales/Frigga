#pragma once

#include <Freyr/Freyr.hpp>

FRIGGA_BEGIN

struct NameComponent: fr::Component
{
    std::string name;

    NameComponent()                      = default;
    NameComponent(const NameComponent &) = default;
    NameComponent(const std::string &name): name(name) {}
};

FRIGGA_END
