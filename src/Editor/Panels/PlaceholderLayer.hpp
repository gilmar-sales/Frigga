#pragma once

#include <Frigga/Core/Layer.hpp>

#include <string>

class PlaceholderLayer: public fg::Layer
{
  public:
    PlaceholderLayer(std::string title, std::string description);
    ~PlaceholderLayer() override = default;

    void onGui() override;

  private:
    std::string mTitle;
    std::string mDescription;
};
