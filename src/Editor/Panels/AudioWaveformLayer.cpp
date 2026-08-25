#include "AudioWaveformLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <algorithm>
#include <cmath>

AudioWaveformLayer::AudioWaveformLayer(skr::Arc<fg::AssetRegistry> assets,
                                       skr::Arc<fg::IAudioEngine> audioEngine)
    : Layer("Waveform"), mAssets(std::move(assets)), mAudioEngine(std::move(audioEngine))
{
}

void AudioWaveformLayer::ensureWaveformLoaded()
{
    if(mSelectedClip.empty())
    {
        mWaveformLoaded = false;
        return;
    }

    if(mWaveformLoaded)
    {
        return;
    }

    const auto absolute = fg::AssetRegistry::ToAbsoluteResourcePath(mSelectedClip);
    if(const auto decoded = mAudioEngine->DecodeWaveform(absolute))
    {
        mWaveform       = *decoded;
        mTrimEndSec     = mWaveform.durationSec;
        mWaveformLoaded = true;
    }
}

void AudioWaveformLayer::onGui()
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

    const auto &clips = mAssets->GetAudioClips();
    if(clips.empty())
    {
        ImGui::TextDisabled("Import .wav/.ogg clips into Resources/Audio/Clips.");
        ImGui::End();
        return;
    }

    if(ImGui::BeginCombo("Clip", mSelectedClip.empty() ? "(select clip)" : mSelectedClip.c_str()))
    {
        for(const auto &clip : clips)
        {
            const bool selected = clip.relativePath == mSelectedClip;
            if(ImGui::Selectable(clip.relativePath.c_str(), selected))
            {
                mSelectedClip   = clip.relativePath;
                mWaveformLoaded = false;
            }
        }
        ImGui::EndCombo();
    }

    ensureWaveformLoaded();

    if(!mWaveformLoaded || mWaveform.peaks.empty())
    {
        ImGui::TextDisabled("No waveform data for selected clip.");
        ImGui::End();
        return;
    }

    ImGui::Text("Duration: %.2f s  |  Rate: %d Hz  |  Channels: %d", mWaveform.durationSec,
                mWaveform.sampleRate, mWaveform.channels);

    const ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 120.0f);
    const ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
    ImDrawList  *drawList   = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos,
                            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(30, 30, 30, 255));

    const float midY = canvasPos.y + canvasSize.y * 0.5f;
    const float scaleX =
        canvasSize.x / static_cast<float>(std::max<std::size_t>(1, mWaveform.peaks.size()));

    for(std::size_t i = 0; i < mWaveform.peaks.size(); ++i)
    {
        const float x     = canvasPos.x + static_cast<float>(i) * scaleX;
        const float peak  = mWaveform.peaks[i];
        const float top   = midY - peak * canvasSize.y * 0.45f;
        const float bottom = midY + peak * canvasSize.y * 0.45f;
        drawList->AddLine(ImVec2(x, top), ImVec2(x, bottom), IM_COL32(120, 180, 255, 255));
    }

    if(mWaveform.durationSec > 0.0f)
    {
        const float startX =
            canvasPos.x + (mTrimStartSec / mWaveform.durationSec) * canvasSize.x;
        const float endX = canvasPos.x + (mTrimEndSec / mWaveform.durationSec) * canvasSize.x;
        drawList->AddLine(ImVec2(startX, canvasPos.y), ImVec2(startX, canvasPos.y + canvasSize.y),
                          IM_COL32(255, 200, 80, 255), 2.0f);
        drawList->AddLine(ImVec2(endX, canvasPos.y), ImVec2(endX, canvasPos.y + canvasSize.y),
                          IM_COL32(255, 120, 80, 255), 2.0f);
    }

    ImGui::Dummy(canvasSize);

    ImGui::SliderFloat("Trim Start", &mTrimStartSec, 0.0f, mWaveform.durationSec, "%.3f s");
    ImGui::SliderFloat("Trim End", &mTrimEndSec, mTrimStartSec, mWaveform.durationSec, "%.3f s");

    if(ImGui::Button("Preview Trim Region"))
    {
        if(const auto *clip = mAssets->FindAudioClip(mSelectedClip))
        {
            (void)clip;
            // Trim markers are stored locally for now; sidecar metadata can be added later.
        }
    }

    ImGui::End();
}
