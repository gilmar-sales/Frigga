#pragma once

#include "Frigga/Events/Event.hpp"

namespace FRIGGA_NAMESPACE
{

    class Layer
    {
      private:
        std::string mName;

      public:
        Layer(const std::string &name = "Layer");
        virtual ~Layer() = default;

        virtual void onAttach() {}
        virtual void onDettach() {}
        virtual void onGui() {}
        virtual void onEvent(Event &event) {}
        virtual void onUpdate() {}

        const std::string &getName() const
        {
            return mName;
        }
    };

} // namespace FRIGGA_NAMESPACE
