#pragma once

#include "Editor/Project/ProjectSession.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Input/InputMap.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <string>
#include <vector>

class InputMapLayer: public fg::Layer
{
  public:
    InputMapLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::Input> input,
                  skr::Arc<fg::SceneSimulationState> simulation);
    ~InputMapLayer() override = default;

    void onUpdate() override;
    void onGui() override;

    static bool IsOpen;

  private:
    enum class SelectionKind
    {
        None,
        Action,
        Axis,
    };

    void syncFromHostIfNeeded();
    void markDirty();
    void applyToHost();
    void saveToDisk();
    void reloadFromDisk();
    void resetDefaults();

    void drawToolbar();
    void drawActionList();
    void drawAxisList();
    void drawActionEditor();
    void drawAxisEditor();
    void drawLivePreview();

    void drawEnumChipList(const char *label, std::vector<fra::KeyCode> &values);
    void drawEnumChipList(const char *label, std::vector<fra::MouseButton> &values);
    void drawEnumChipList(const char *label, std::vector<fra::GamepadButton> &values);
    void drawKeyAddRow(const char *id, std::vector<fra::KeyCode> &values);
    void drawMouseAddRow(const char *id, std::vector<fra::MouseButton> &values);
    void drawGamepadAddRow(const char *id, std::vector<fra::GamepadButton> &values);

    [[nodiscard]] std::vector<std::string> sortedActionNames() const;
    [[nodiscard]] std::vector<std::string> sortedAxisNames() const;

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::Input> mInput;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    fg::InputMap mDraft {};
    std::string mProjectRootKey;
    bool mDirty           = false;
    bool mWasOpen         = false;
    SelectionKind mKind   = SelectionKind::None;
    std::string mSelected;

    char mNewActionName[64] = {};
    char mNewAxisName[64]   = {};
    char mRenameBuf[64]     = {};

    std::string mStatus;
    std::string mError;

    int mAddKeyIndex     = 0;
    int mAddMouseIndex   = 0;
    int mAddGamepadIndex = 0;
    int mAddNegKeyIndex  = 0;
    int mAddPosKeyIndex  = 0;
};
