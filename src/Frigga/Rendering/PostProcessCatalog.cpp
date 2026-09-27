#include <Frigga/Rendering/PostProcessCatalog.hpp>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        struct OutlinePush
        {
            float     edgeDepthScale  = 80.0f;
            float     edgeNormalScale = 2.0f;
            float     strength        = 1.0f;
            float     reverseZ        = 0.0f;
            glm::vec4 edgeColor {0.02f, 0.02f, 0.04f, 1.0f};
            float     edgeWidth = 1.0f;
            float     _pad0     = 0.0f;
            float     _pad1     = 0.0f;
            float     _pad2     = 0.0f;
        };

        struct GradePush
        {
            float     contrast   = 1.05f;
            float     saturation = 1.15f;
            float     exposure   = 0.0f;
            float     vignette   = 0.35f;
            glm::vec4 lift {0.0f};
            glm::vec4 gain {1.0f, 1.0f, 1.0f, 1.0f};
        };

        struct UnderwaterPush
        {
            float     time         = 0.0f;
            float     strength     = 1.0f;
            float     tintStrength = 0.55f;
            float     fogDensity   = 1.8f;
            glm::vec4 tintColor {0.15f, 0.45f, 0.55f, 1.0f};
            float     reverseZ = 0.0f;
            float     maxDepth = 0.85f;
            float     _pad0    = 0.0f;
            float     _pad1    = 0.0f;
        };

        struct HeatPush
        {
            float time     = 0.0f;
            float strength = 1.0f;
            float speed    = 1.2f;
            float reverseZ = 0.0f;
        };

        struct GlowPush
        {
            float     intensity = 2.2f;
            float     radius    = 8.0f;
            float     fill      = 0.25f;
            float     reverseZ  = 0.0f;
            glm::vec4 color {1.0f, 0.85f, 0.25f, 1.0f};
        };

        struct MuGlowPush
        {
            float time      = 0.0f;
            float level     = 13.0f;
            float intensity = 1.0f;
            float reverseZ  = 0.0f;
            float radius    = 7.0f;
            float waveSpeed = 1.0f;
            float _pad0     = 0.0f;
            float _pad1     = 0.0f;
        };

        struct CellPushConstants
        {
            float     bands           = 4.0f;
            float     edgeDepthScale  = 80.0f;
            float     edgeNormalScale = 2.0f;
            float     strength        = 1.0f;
            glm::vec4 edgeColor {0.02f, 0.02f, 0.04f, 1.0f};
            float     reverseZ   = 0.0f;
            float     shadowLift = 0.22f;
            float     edgeWidth  = 1.0f;
        };

        [[nodiscard]] bool UsesSceneDepthNormal(PostProcessKind kind)
        {
            switch(kind)
            {
            case PostProcessKind::Cell:
            case PostProcessKind::Outline:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] bool UsesSceneDepth(PostProcessKind kind)
        {
            switch(kind)
            {
            case PostProcessKind::Underwater:
            case PostProcessKind::HeatHaze:
            case PostProcessKind::Glow:
            case PostProcessKind::MuItemGlow:
                return true;
            default:
                return UsesSceneDepthNormal(kind);
            }
        }
    } // namespace

    std::string PostProcessFragmentPath(const PostProcessKind kind,
                                        const std::string_view customFragment)
    {
        switch(kind)
        {
        case PostProcessKind::Cell:
            return "Cell/cell.frag.spv";
        case PostProcessKind::Outline:
            return "Post/outline.frag.spv";
        case PostProcessKind::ColorGrade:
            return "Post/color_grade.frag.spv";
        case PostProcessKind::Underwater:
            return "Post/underwater.frag.spv";
        case PostProcessKind::HeatHaze:
            return "Post/heat_haze.frag.spv";
        case PostProcessKind::Glow:
            return "Post/glow.frag.spv";
        case PostProcessKind::MuItemGlow:
            return "Post/mu_item_glow.frag.spv";
        case PostProcessKind::Custom:
            return std::string(customFragment);
        }
        return std::string(customFragment);
    }

    void ConfigurePostProcessBuilder(fra::PostProcessBuilder &builder,
                                     const std::string_view stageName,
                                     const PostProcessComponent &component)
    {
        const std::string fragment = PostProcessFragmentPath(component.kind, component.fragment);
        builder.SetName(std::string(stageName)).SetFragment(fragment);

        std::vector<fra::PostProcessInput> inputs {fra::PostProcessInput::SceneColor};
        if(UsesSceneDepthNormal(component.kind))
        {
            inputs = {fra::PostProcessInput::SceneColor, fra::PostProcessInput::Depth,
                      fra::PostProcessInput::Normal};
        }
        else if(UsesSceneDepth(component.kind))
        {
            inputs = {fra::PostProcessInput::SceneColor, fra::PostProcessInput::Depth};
        }

        std::uint32_t pushSize = 0;
        switch(component.kind)
        {
        case PostProcessKind::Cell:
            pushSize = static_cast<std::uint32_t>(sizeof(CellPushConstants));
            break;
        case PostProcessKind::Outline:
            pushSize = static_cast<std::uint32_t>(sizeof(OutlinePush));
            break;
        case PostProcessKind::ColorGrade:
            pushSize = static_cast<std::uint32_t>(sizeof(GradePush));
            break;
        case PostProcessKind::Underwater:
            pushSize = static_cast<std::uint32_t>(sizeof(UnderwaterPush));
            break;
        case PostProcessKind::HeatHaze:
            pushSize = static_cast<std::uint32_t>(sizeof(HeatPush));
            break;
        case PostProcessKind::Glow:
            pushSize = static_cast<std::uint32_t>(sizeof(GlowPush));
            break;
        case PostProcessKind::MuItemGlow:
            pushSize = static_cast<std::uint32_t>(sizeof(MuGlowPush));
            break;
        case PostProcessKind::Custom:
            if(fragment.find("cell.frag") != std::string::npos)
            {
                pushSize = static_cast<std::uint32_t>(sizeof(CellPushConstants));
                inputs   = {fra::PostProcessInput::SceneColor, fra::PostProcessInput::Depth,
                            fra::PostProcessInput::Normal};
            }
            break;
        }

        builder.SetInputs(std::move(inputs)).SetPushConstantSize(pushSize);
    }

    void ApplyPostProcessPushConstants(fra::PostProcess &effect,
                                       const PostProcessPushState &state)
    {
        if(state.component == nullptr)
        {
            return;
        }

        const auto &component = *state.component;
        const float revZ      = state.reverseZ ? 1.0f : 0.0f;

        switch(component.kind)
        {
        case PostProcessKind::Cell:
        {
            CellPushConstants cell {};
            cell.bands           = component.bands;
            cell.edgeDepthScale  = component.edgeDepthScale;
            cell.edgeNormalScale = component.edgeNormalScale;
            cell.strength        = component.strength;
            cell.edgeColor       = component.edgeColor;
            cell.reverseZ        = revZ;
            cell.shadowLift      = component.shadowLift;
            cell.edgeWidth       = component.edgeWidth;
            effect.SetPushConstants(cell);
            break;
        }
        case PostProcessKind::Outline:
        {
            OutlinePush push {};
            push.edgeDepthScale  = component.edgeDepthScale;
            push.edgeNormalScale = component.edgeNormalScale;
            push.strength        = component.strength;
            push.reverseZ        = revZ;
            push.edgeColor       = component.edgeColor;
            push.edgeWidth       = component.edgeWidth;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::ColorGrade:
        {
            GradePush push {};
            push.contrast   = component.contrast;
            push.saturation = component.saturation;
            push.exposure   = component.exposure;
            push.vignette   = component.vignette;
            push.lift       = component.lift;
            push.gain       = component.gain;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::Underwater:
        {
            UnderwaterPush push {};
            push.time         = state.timeSec;
            push.strength     = component.strength;
            push.tintStrength = component.tintStrength;
            push.fogDensity   = component.fogDensity;
            push.tintColor    = component.tintColor;
            push.reverseZ     = revZ;
            push.maxDepth     = component.maxDepth;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::HeatHaze:
        {
            HeatPush push {};
            push.time     = state.timeSec;
            push.strength = component.strength;
            push.speed    = component.heatSpeed;
            push.reverseZ = revZ;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::Glow:
        {
            GlowPush push {};
            push.intensity = component.glowIntensity;
            push.radius    = component.glowRadius;
            push.fill      = component.glowFill;
            push.reverseZ  = revZ;
            push.color     = component.glowColor;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::MuItemGlow:
        {
            MuGlowPush push {};
            push.time      = state.timeSec;
            push.level     = component.muGlowLevel;
            push.intensity = component.glowIntensity;
            push.reverseZ  = revZ;
            push.radius    = component.glowRadius;
            push.waveSpeed = component.heatSpeed;
            effect.SetPushConstants(push);
            break;
        }
        case PostProcessKind::Custom:
            if(component.fragment.find("cell.frag") != std::string::npos)
            {
                CellPushConstants cell {};
                cell.bands           = component.bands;
                cell.edgeDepthScale  = component.edgeDepthScale;
                cell.edgeNormalScale = component.edgeNormalScale;
                cell.strength        = component.strength;
                cell.edgeColor       = component.edgeColor;
                cell.reverseZ        = revZ;
                cell.shadowLift      = component.shadowLift;
                cell.edgeWidth       = component.edgeWidth;
                effect.SetPushConstants(cell);
            }
            break;
        }
    }

    void SyncPostProcessMaterials(fra::PostProcess &effect,
                                  const PostProcessComponent &component)
    {
        effect.ClearMaterials();
        for(const auto materialId : component.materialMaskIds)
        {
            effect.BindMaterial(materialId);
        }
    }

} // namespace FRIGGA_NAMESPACE
