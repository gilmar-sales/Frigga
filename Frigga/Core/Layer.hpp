#pragma once

#include "Events/Event.hpp"

FRIGGA_BEGIN

    class Layer
    {
    private:
        std::string m_name;
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer() = default;

        virtual void onAttach() {}
        virtual void onDettach() {}
        virtual void onGui() {}
        virtual void onEvent(Event& event) {}
        virtual void onUpdate() {}

        const std::string& getName() const { return m_name; }
    };

FRIGGA_END
