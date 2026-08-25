#pragma once

#include "Frigga/Audio/IAudioEngine.hpp"

#include <Frigga/Core/Layer.hpp>

class AudioMixerLayer: public fg::Layer
{
  public:
    explicit AudioMixerLayer(skr::Arc<fg::IAudioEngine> audioEngine);
    ~AudioMixerLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::IAudioEngine> mAudioEngine;
};
