#pragma once

#include "Design.hpp"

#include "Systems.hpp"

using SystemList = std::tuple<fg::PhysicsSystem, fg::RenderSystem>;

#include <Freyr/Core/World.hpp>

using World = freyr::World;