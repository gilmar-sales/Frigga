#pragma once

#include "Components.hpp"
#include "Tags.hpp"

#include <Freyr/Core/Design.hpp>

FRIGGA_BEGIN

using ComponentList = std::tuple<NameComponent, TransformComponent, MeshComponent>;
using TagList       = std::tuple<PlayerTag>;

FRIGGA_END

using Design = freyr::Design<fg::ComponentList, fg::TagList>;
