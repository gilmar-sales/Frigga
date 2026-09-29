#pragma once

#include "Frigga/Asset/AssetFwd.hpp"
#include "Frigga/Audio/AudioBankDefinition.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>

#include <filesystem>
#include <string>
#include <vector>

/// Editor for `.audiobank.json` files: browse the banks under Resources/Audio/Banks,
/// edit their events (clip / volume / pitch / loop / bus) and save them back, which
/// reloads the bank in the audio engine.
class AudioBanksLayer: public fg::Layer
{
  public:
    AudioBanksLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<fg::IAudioEngine> audioEngine,
                    skr::Arc<fg::AudioController> controller,
                    skr::Arc<fg::SceneSimulationState> simulation);
    ~AudioBanksLayer() override = default;

    void onGui() override;

  private:
    void refreshBankList();
    void selectBank(const std::filesystem::path &relativePath);
    void drawToolbar(bool playing);
    void drawBankList(bool playing);
    void drawEventEditor(bool playing);
    void createBank();

    skr::Arc<fg::AssetRegistry>        mAssets;
    skr::Arc<fg::IAudioEngine>         mAudioEngine;
    skr::Arc<fg::AudioController>      mController;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    std::vector<std::filesystem::path> mBankFiles;
    std::string                        mSelectedBank;
    fg::AudioBankDefinition            mDefinition;
    bool                               mLoaded    = false;
    bool                               mDirty     = false;
    bool                               mListDirty = true;
    std::string                        mStatus;
    char                               mNewBankName[96] {};
    char                               mFilter[96] {};
};
