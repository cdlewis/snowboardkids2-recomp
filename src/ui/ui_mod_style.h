#ifndef RECOMPUI_MOD_STYLE_H
#define RECOMPUI_MOD_STYLE_H

#include <string>

#include "elements/ui_button.h"
#include "elements/ui_types.h"

namespace recompui {

struct ModThumbnailRef {
    std::string src;
    bool has_image = false;
};

namespace mod_style {

inline constexpr Color config_body_color{ 73, 138, 223, 255 };
inline constexpr Color config_bar_color{ 85, 145, 223, 255 };
inline constexpr Color action_button_color{ 137, 180, 232, 255 };
inline constexpr Color action_button_border_color{ 143, 186, 237, 255 };
inline constexpr Color action_button_hover_color{ 122, 42, 198, 255 };
inline constexpr Color transparent_color{ 0, 0, 0, 0 };
inline constexpr Color list_thumbnail_placeholder_color{ 190, 184, 219, 25 };
inline constexpr Color details_thumbnail_placeholder_color{ 255, 255, 255, 25 };

inline void apply_action_button_style(Button* button) {
    button->add_class("button");
    button->add_class("mod-footer__button");
    button->set_color(Color{ 255, 255, 255, 255 });
    button->set_border_width(2.0f);
    button->set_border_color(action_button_border_color);
    button->set_background_color(action_button_color);

    button->get_hover_style()->set_color(Color{ 255, 255, 255, 255 });
    button->get_hover_style()->set_border_color(action_button_hover_color);
    button->get_hover_style()->set_background_color(action_button_hover_color);
    button->get_focus_style()->set_color(Color{ 255, 255, 255, 255 });
    button->get_focus_style()->set_border_color(action_button_hover_color);
    button->get_focus_style()->set_background_color(action_button_hover_color);

    button->get_disabled_style()->set_color(Color{ 255, 255, 255, 153 });
    button->get_disabled_style()->set_border_color(Color{ 143, 186, 237, 64 });
    button->get_disabled_style()->set_background_color(Color{ 137, 180, 232, 64 });
    button->get_hover_disabled_style()->set_color(Color{ 255, 255, 255, 153 });
    button->get_hover_disabled_style()->set_border_color(Color{ 143, 186, 237, 64 });
    button->get_hover_disabled_style()->set_background_color(Color{ 137, 180, 232, 64 });
}

inline void apply_action_button_normal_focus_style(Button* button) {
    button->get_focus_style()->set_border_color(action_button_border_color);
    button->get_focus_style()->set_background_color(action_button_color);
}

inline void apply_header_gear_button_style(Button* button) {
    button->set_position(Position::Relative);
    button->set_top(-2.0f);
    button->set_width(72.0f);
    button->set_height(72.0f);
    button->set_padding(0.0f);
    button->set_border_radius(36.0f);
    button->set_border_color(transparent_color);
    button->set_background_color(transparent_color);
    button->set_color(Color{ 255, 255, 255, 255 });
    button->set_font_family("promptfont");
    button->set_font_size(40.0f);
    button->set_letter_spacing(0.0f);
    button->set_line_height(72.0f);
    button->set_text_align(TextAlign::Center);

    button->get_hover_style()->set_border_color(transparent_color);
    button->get_hover_style()->set_background_color(Color{ 255, 255, 255, 51 });
    button->get_hover_style()->set_color(Color{ 255, 255, 255, 255 });
    button->get_focus_style()->set_border_color(transparent_color);
    button->get_focus_style()->set_background_color(transparent_color);
    button->get_focus_style()->set_color(Color{ 255, 255, 255, 255 });
    button->get_disabled_style()->set_border_color(transparent_color);
    button->get_disabled_style()->set_background_color(transparent_color);
    button->get_disabled_style()->set_color(Color{ 255, 255, 255, 128 });
    button->get_hover_disabled_style()->set_border_color(transparent_color);
    button->get_hover_disabled_style()->set_background_color(transparent_color);
    button->get_hover_disabled_style()->set_color(Color{ 255, 255, 255, 128 });
}

} // namespace mod_style

} // namespace recompui

#endif
