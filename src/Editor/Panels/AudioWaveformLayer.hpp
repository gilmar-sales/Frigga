#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"

#include <Frigga/Core/Layer.hpp>

class AudioWaveformLayer: public fg::Layer
{
  public:
    AudioWaveformLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<fg::IAudioEngine> audioEngine,
                       skr::Arc<fg::AudioController> controller);
    ~AudioWaveformLayer() override = default;

    void onGui() override;

  private:
    void ensureWaveformLoaded();

    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::IAudioEngine> mAudioEngine;
    skr::Arc<fg::AudioController> mController;
    std::string mSelectedClip;
    fg::WaveformData mWaveform {};
    bool mWaveformLoaded = false;
    float mTrimStartSec = 0.0f;
    float mTrimEndSec   = 0.0f;
};
