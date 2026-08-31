#include "AudioMixerLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <cstdio>
#include <cstring>

AudioMixerLayer::AudioMixerLayer(skr::Arc<fg::IAudioEngine> audioEngine)
    : Layer("Mixer"), mAudioEngine(std::move(audioEngine))
{
}

void AudioMixerLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    if(!mAudioEngine->IsInitialized())
    {
        (void)mAudioEngine->Initialize();
    }

    ImGui::SeparatorText("Buses");
    for(const auto &bus : mAudioEngine->GetMixerBuses())
    {
        ImGui::PushID(bus.path.c_str());
        float volume = bus.volume;
        if(ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f"))
        {
            mAudioEngine->SetBusVolume(bus.path, volume);
        }
        bool muted = bus.muted;
        if(ImGui::Checkbox("Mute", &muted))
        {
            mAudioEngine->SetBusMute(bus.path, muted);
        }
        ImGui::TextDisabled("%s", bus.path.c_str());
        ImGui::PopID();
        ImGui::Separator();
    }

    ImGui::SeparatorText("VCAs");
    for(const auto &vca : mAudioEngine->GetMixerVcas())
    {
        ImGui::PushID(vca.path.c_str());
        float volume = vca.volume;
        if(ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f"))
        {
            mAudioEngine->SetVcaVolume(vca.path, volume);
        }
        ImGui::TextDisabled("%s", vca.path.c_str());
        ImGui::PopID();
    }

    ImGui::End();
}
