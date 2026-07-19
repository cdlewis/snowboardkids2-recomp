#include "sk2_theme.h"

#include "elements/ui_modal.h"
#include "elements/ui_frontend_theme.h"
#include "elements/ui_theme.h"
#include "recompui/config.h"
#include "recompui/recompui.h"

namespace ui_theme = recompui::theme;

namespace sk2::theme {

void apply() {
    using namespace recompui;
    using ui_theme::color;

    register_primary_font("Fredoka.ttf", "Fredoka");
    register_extra_font("LatoLatin-Regular.ttf");
    register_extra_font("LatoLatin-Italic.ttf");
    register_extra_font("LatoLatin-Bold.ttf");
    register_extra_font("LatoLatin-BoldItalic.ttf");

    const Color blue{ 47, 149, 224, 255 };
    const Color blue_light{ 78, 166, 232, 255 };
    const Color blue_button{ 119, 183, 235, 255 };
    const Color purple_accent{ 179, 130, 245, 255 };

    ui_theme::FrontendTheme frontend_theme = ui_theme::make_default_frontend_theme();
    auto set_color = [&frontend_theme](ui_theme::color id, Color value) {
        frontend_theme.colors[static_cast<std::size_t>(id)] = value;
    };

    set_color(color::Background1, blue);
    set_color(color::Background2, blue);
    set_color(color::Background3, blue_light);
    set_color(color::BGOverlay, blue_light);
    set_color(color::ModalOverlay, blue);
    set_color(color::BGShadow, Color{ 0, 0, 0, 0 });
    set_color(color::BGShadow2, blue);

    set_color(color::Text, Color{ 255, 255, 255, 255 });
    set_color(color::TextActive, purple_accent);
    set_color(color::TextDim, Color{ 220, 235, 250, 255 });
    set_color(color::TextInactive, Color{ 255, 255, 255, 220 });
    set_color(color::TextA80, Color{ 255, 255, 255, 204 });

    set_color(color::Primary, Color{ 255, 255, 255, 255 });
    set_color(color::PrimaryL, Color{ 255, 255, 255, 255 });
    set_color(color::PrimaryD, Color{ 220, 235, 250, 255 });
    set_color(color::PrimaryA5, Color{ 255, 255, 255, 13 });
    set_color(color::PrimaryA20, Color{ 255, 255, 255, 51 });
    set_color(color::PrimaryA30, Color{ 255, 255, 255, 77 });
    set_color(color::PrimaryA50, Color{ 255, 255, 255, 128 });
    set_color(color::PrimaryA80, Color{ 255, 255, 255, 204 });

    set_color(color::Secondary, purple_accent);
    set_color(color::SecondaryL, Color{ 217, 194, 251, 255 });
    set_color(color::SecondaryD, Color{ 145, 96, 211, 255 });
    set_color(color::SecondaryA5, Color{ 179, 130, 245, 13 });
    set_color(color::SecondaryA20, Color{ 179, 130, 245, 51 });
    set_color(color::SecondaryA30, Color{ 179, 130, 245, 77 });
    set_color(color::SecondaryA50, Color{ 179, 130, 245, 128 });
    set_color(color::SecondaryA80, Color{ 179, 130, 245, 204 });

    set_color(color::Border, Color{ 115, 190, 244, 255 });
    set_color(color::BorderSoft, Color{ 255, 255, 255, 35 });
    set_color(color::BorderHard, Color{ 255, 255, 255, 95 });
    set_color(color::BorderSolid, blue_button);

    set_color(color::BW25, Color{ 119, 183, 235, 128 });
    set_color(color::BW50, Color{ 119, 183, 235, 255 });

    set_color(color::Warning, Color{ 255, 255, 255, 255 });
    set_color(color::WarningL, Color{ 255, 255, 255, 255 });
    set_color(color::WarningD, Color{ 220, 235, 250, 255 });
    set_color(color::WarningA5, Color{ 255, 255, 255, 13 });
    set_color(color::WarningA20, Color{ 255, 255, 255, 51 });
    set_color(color::WarningA30, Color{ 255, 255, 255, 77 });
    set_color(color::WarningA50, Color{ 255, 255, 255, 128 });
    set_color(color::WarningA80, Color{ 255, 255, 255, 204 });

    set_color(color::Elevated, Color{ 119, 183, 235, 220 });
    set_color(color::ElevatedSoft, Color{ 119, 183, 235, 205 });
    set_color(color::ElevatedBorder, Color{ 178, 218, 248, 180 });
    set_color(color::ElevatedBorderHard, Color{ 217, 238, 255, 230 });

    frontend_theme.primary_button = ui_theme::ButtonStylePalette{
        .text = Color{ 255, 255, 255, 255 },
        .background = blue_button,
        .border = Color{ 143, 186, 237, 255 },
        .active_text = Color{ 255, 255, 255, 255 },
        .active_background = purple_accent,
        .active_border = purple_accent,
        .disabled_text = Color{ 220, 235, 250, 128 },
    };
    frontend_theme.secondary_button = ui_theme::ButtonStylePalette{
        .text = Color{ 255, 255, 255, 255 },
        .background = Color{ 0, 0, 0, 0 },
        .border = Color{ 178, 218, 248, 180 },
        .active_text = Color{ 255, 255, 255, 255 },
        .active_background = Color{ 146, 204, 250, 255 },
        .active_border = Color{ 217, 238, 255, 230 },
        .disabled_text = Color{ 220, 235, 250, 128 },
    };

    frontend_theme.modal.overlay.set_background_color(Color{ 0, 0, 0, 77 });
    frontend_theme.modal.overlay.set_font_family("Fredoka");
    frontend_theme.modal.frame.set_background_color(Color{ 78, 147, 218, 255 });
    frontend_theme.modal.frame.set_border_color(Color{ 0, 0, 0, 255 });
    frontend_theme.modal.frame.set_font_family("Fredoka");
    frontend_theme.modal.frame.set_border_width(7.0f);
    frontend_theme.modal.frame.set_border_radius(64.0f);
    frontend_theme.modal.frame.set_padding_top(42.0f);
    frontend_theme.modal.frame.set_padding_right(56.0f);
    frontend_theme.modal.frame.set_padding_bottom(54.0f);
    frontend_theme.modal.frame.set_padding_left(56.0f);
    frontend_theme.modal.header.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.modal.header.set_border_bottom_color(Color{ 255, 255, 255, 51 });
    frontend_theme.modal.header.set_border_bottom_width(5.0f);
    frontend_theme.modal.body.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.modal.tabs.emplace();
    frontend_theme.modal.tabs->normal.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.modal.tabs->hover.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.modal.tabs->selected.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.modal.tabs->selected.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.modal.tabs->pulsing.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.modal.tabs->indicator.set_height(5.0f);
    frontend_theme.modal.tabs->indicator.set_bottom(-1.5f);
    frontend_theme.modal.tabs->indicator_color = Color{ 242, 242, 242, 255 };

    frontend_theme.prompt.overlay.set_background_color(Color{ 0, 0, 0, 77 });
    frontend_theme.prompt.content.set_background_color(Color{ 0, 0, 0, 204 });
    frontend_theme.prompt.content.set_border_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.content.set_border_width(7.0f);
    frontend_theme.prompt.content.set_border_radius(48.0f);
    frontend_theme.prompt.content.set_font_family("Fredoka");
    frontend_theme.prompt.controls.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.prompt.controls.set_border_top_color(Color{ 255, 255, 255, 64 });
    frontend_theme.prompt.controls.set_border_bottom_left_radius(48.0f);
    frontend_theme.prompt.controls.set_border_bottom_right_radius(48.0f);
    frontend_theme.prompt.controls.set_justify_content(JustifyContent::Center);
    frontend_theme.prompt.button.normal.set_margin_left(12.0f);
    frontend_theme.prompt.button.normal.set_margin_right(12.0f);
    frontend_theme.prompt.button.normal.set_color(Color{ 0, 0, 0, 255 });
    frontend_theme.prompt.button.normal.set_background_color(Color{ 242, 242, 242, 255 });
    frontend_theme.prompt.button.normal.set_border_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.button.normal.set_font_family("Fredoka");
    frontend_theme.prompt.button.hover.set_color(Color{ 0, 0, 0, 255 });
    frontend_theme.prompt.button.hover.set_background_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.button.hover.set_border_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.button.focus.apply_style(frontend_theme.prompt.button.hover);
    frontend_theme.prompt.secondary_button.emplace();
    frontend_theme.prompt.secondary_button->normal.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.secondary_button->normal.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.prompt.secondary_button->normal.set_border_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.secondary_button->hover.set_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.secondary_button->hover.set_background_color(Color{ 255, 255, 255, 51 });
    frontend_theme.prompt.secondary_button->hover.set_border_color(Color{ 255, 255, 255, 255 });
    frontend_theme.prompt.secondary_button->focus.apply_style(frontend_theme.prompt.secondary_button->hover);

    frontend_theme.controls.binding_button.normal.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.binding_button.normal.set_border_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.binding_button.hover.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.binding_button.hover.set_border_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.binding_button.focus.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.binding_button.focus.set_border_color(Color{ 0, 0, 0, 0 });
    frontend_theme.controls.input_device_toggle = ui_theme::TogglePreset::Filled;

    auto& in_game_assignment = frontend_theme.in_game_player_assignment;
    in_game_assignment.page.set_background_color(Color{ 0, 0, 0, 255 });
    in_game_assignment.page.set_font_family("Fredoka");
    in_game_assignment.content.set_background_color(Color{ 0, 0, 0, 255 });
    in_game_assignment.content.set_border_width(7.0f);
    in_game_assignment.content.set_border_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.content.set_border_radius(64.0f);
    in_game_assignment.content.set_padding_top(20.0f);
    in_game_assignment.content.set_padding_right(56.0f);
    in_game_assignment.content.set_padding_bottom(20.0f);
    in_game_assignment.content.set_padding_left(56.0f);
    in_game_assignment.heading.set_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.heading.set_text_align(TextAlign::Center);
    in_game_assignment.description.set_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.description.set_text_align(TextAlign::Center);
    in_game_assignment.card_border = Color{ 255, 255, 255, 128 };
    in_game_assignment.card_label = Color{ 255, 255, 255, 204 };
    in_game_assignment.card_waiting = Color{ 255, 255, 255, 128 };
    in_game_assignment.card_active = Color{ 255, 255, 255, 255 };
    in_game_assignment.card_assigned = Color{ 255, 255, 255, 255 };
    in_game_assignment.card_assigned_background = Color{ 255, 255, 255, 51 };
    in_game_assignment.button.normal.set_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.button.normal.set_background_color(Color{ 0, 0, 0, 255 });
    in_game_assignment.button.normal.set_border_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.button.hover.set_color(Color{ 0, 0, 0, 255 });
    in_game_assignment.button.hover.set_background_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.button.hover.set_border_color(Color{ 255, 255, 255, 255 });
    in_game_assignment.button.focus.apply_style(in_game_assignment.button.hover);
    in_game_assignment.button.disabled.set_color(Color{ 255, 255, 255, 96 });
    in_game_assignment.button.disabled.set_background_color(Color{ 0, 0, 0, 255 });
    in_game_assignment.button.disabled.set_border_color(Color{ 255, 255, 255, 64 });

    frontend_theme.mods.details.set_background_color(Color{ 0, 0, 0, 0 });
    frontend_theme.mods.enable_toggle = ui_theme::TogglePreset::Filled;

    ui_theme::set_frontend_theme(frontend_theme);
}

} // namespace sk2::theme
