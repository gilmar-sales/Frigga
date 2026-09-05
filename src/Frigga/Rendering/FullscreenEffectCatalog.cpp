#include <Frigga/Rendering/FullscreenEffectCatalog.hpp>

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

        [[nodiscard]] bool UsesSceneDepthNormal(FullscreenEffectKind kind)
        {
            switch(kind)
            {
            case FullscreenEffectKind::Cell:
            case FullscreenEffectKind::Outline:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] bool UsesSceneDepth(FullscreenEffectKind kind)
        {
            switch(kind)
            {
            case FullscreenEffectKind::Underwater:
            case FullscreenEffectKind::HeatHaze:
            case FullscreenEffectKind::Glow:
            case FullscreenEffectKind::MuItemGlow:
                return true;
            default:
                return UsesSceneDepthNormal(kind);
            }
        }
    } // namespace

    std::string FullscreenEffectFragmentPath(const FullscreenEffectKind kind,
                                             const std::string_view customFragment)
    {
        switch(kind)
        {
        case FullscreenEffectKind::Cell:
            return "Cell/cell.frag.spv";
        case FullscreenEffectKind::Outline:
            return "Post/outline.frag.spv";
        case FullscreenEffectKind::ColorGrade:
            return "Post/color_grade.frag.spv";
        case FullscreenEffectKind::Underwater:
            return "Post/underwater.frag.spv";
        case FullscreenEffectKind::HeatHaze:
            return "Post/heat_haze.frag.spv";
        case FullscreenEffectKind::Glow:
            return "Post/glow.frag.spv";
        case FullscreenEffectKind::MuItemGlow:
            return "Post/mu_item_glow.frag.spv";
        case FullscreenEffectKind::Custom:
            return std::string(customFragment);
        }
        return std::string(customFragment);
    }

    void ConfigureFullscreenEffectBuilder(fra::PostProcessBuilder &builder,
                                          const std::string_view stageName,
                                          const FullscreenEffectComponent &component)
    {
        const std::string fragment =
            FullscreenEffectFragmentPath(component.kind, component.fragment);
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
        case FullscreenEffectKind::Cell:
            pushSize = static_cast<std::uint32_t>(sizeof(CellPushConstants));
            break;
        case FullscreenEffectKind::Outline:
            pushSize = static_cast<std::uint32_t>(sizeof(OutlinePush));
            break;
        case FullscreenEffectKind::ColorGrade:
            pushSize = static_cast<std::uint32_t>(sizeof(GradePush));
            break;
        case FullscreenEffectKind::Underwater:
            pushSize = static_cast<std::uint32_t>(sizeof(UnderwaterPush));
            break;
        case FullscreenEffectKind::HeatHaze:
            pushSize = static_cast<std::uint32_t>(sizeof(HeatPush));
            break;
        case FullscreenEffectKind::Glow:
            pushSize = static_cast<std::uint32_t>(sizeof(GlowPush));
            break;
        case FullscreenEffectKind::MuItemGlow:
            pushSize = static_cast<std::uint32_t>(sizeof(MuGlowPush));
            break;
        case FullscreenEffectKind::Custom:
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

    void ApplyFullscreenEffectPushConstants(fra::PostProcess &effect,
                                            const FullscreenEffectPushState &state)
    {
        if(state.component == nullptr)
        {
            return;
        }

        const auto &component = *state.component;
        const float revZ      = state.reverseZ ? 1.0f : 0.0f;

        switch(component.kind)
        {
        case FullscreenEffectKind::Cell:
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
        case FullscreenEffectKind::Outline:
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
        case FullscreenEffectKind::ColorGrade:
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
        case FullscreenEffectKind::Underwater:
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
        case FullscreenEffectKind::HeatHaze:
        {
            HeatPush push {};
            push.time     = state.timeSec;
            push.strength = component.strength;
            push.speed    = component.heatSpeed;
            push.reverseZ = revZ;
            effect.SetPushConstants(push);
            break;
        }
        case FullscreenEffectKind::Glow:
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
        case FullscreenEffectKind::MuItemGlow:
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
        case FullscreenEffectKind::Custom:
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

    void SyncFullscreenEffectMaterials(fra::PostProcess &effect,
                                       const FullscreenEffectComponent &component)
    {
        effect.ClearMaterials();
        for(const auto materialId : component.materialMaskIds)
        {
            effect.BindMaterial(materialId);
        }
    }

} // namespace FRIGGA_NAMESPACE
