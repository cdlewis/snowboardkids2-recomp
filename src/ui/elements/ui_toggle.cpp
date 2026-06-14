#include "ui_toggle.h"

#include <cassert>

#include <ultramodern/ultramodern.hpp>

namespace recompui {

namespace {
    bool move_towards(float& value, float target, float max_delta) {
        if (target < value) {
            value += std::max(-max_delta, target - value);
        }
        else {
            value += std::min(max_delta, target - value);
        }

        if (abs(target - value) < 1e-4f) {
            value = target;
            return false;
        }

        return true;
    }
}

    Toggle::Toggle(Element *parent) : Element(parent, Events(EventType::Click, EventType::Focus, EventType::Hover, EventType::Enable), "button") {
        enable_focus();

        set_width(208.0f);
        set_height(60.0f);
        set_border_radius(16.0f);
        set_cursor(Cursor::Pointer);
        set_border_width(0.0f);
        set_background_color(Color{ 137, 180, 232, 255 });
        hover_style.set_background_color(Color{ 137, 180, 232, 255 });
        focus_style.set_background_color(Color{ 137, 180, 232, 255 });
        checked_hover_style.set_background_color(Color{ 137, 180, 232, 255 });
        checked_focus_style.set_background_color(Color{ 137, 180, 232, 255 });
        add_style(&checked_style, checked_state);
        add_style(&hover_style, hover_state);
        add_style(&focus_style, focus_state);
        add_style(&checked_hover_style, { checked_state, hover_state });
        add_style(&checked_focus_style, { checked_state, focus_state });
        add_style(&disabled_style, disabled_state);
        add_style(&checked_disabled_style, { checked_state, disabled_state });

        ContextId context = get_current_context();

        floater = context.create_element<Element>(this);
        floater->set_position(Position::Relative);
        floater->set_top(4.0f);
        floater->set_width(100.0f);
        floater->set_height(52.0f);
        floater->set_border_radius(12.0f);
        floater->set_background_color(Color{ 122, 42, 198, 255 });
        floater->set_decorator("horizontal-gradient(rgba(51, 90, 199, 0.3), rgba(51, 90, 199, 1))");
        floater_disabled_style.set_background_color(Color{ 122, 42, 198, 128 });
        floater_disabled_checked_style.set_background_color(Color{ 122, 42, 198, 128 });
        floater->add_style(&floater_checked_style, checked_state);
        floater->add_style(&floater_disabled_style, disabled_state);
        floater->add_style(&floater_disabled_checked_style, { checked_state, disabled_state });

        set_checked_internal(false, false, true, false);
    }

    void Toggle::set_checked_internal(bool checked, bool animate, bool setup, bool trigger_callbacks) {
        if (this->checked != checked || setup) {
            this->checked = checked;

            if (animate) {
                last_time = ultramodern::time_since_start();
                queue_update();
            }
            else {
                floater_left = floater_left_target();
                floater_opacity = floater_opacity_target();
            }

            floater->set_left(floater_left, Unit::Dp);
            floater->set_opacity(floater_opacity);

            if (trigger_callbacks) {
                for (const auto &function : checked_callbacks) {
                    function(checked);
                }
            }

            set_style_enabled(checked_state, checked);
            floater->set_style_enabled(checked_state, checked);
        }
    }

    float Toggle::floater_left_target() const {
        return checked ? 100.0f : 6.0f;
    }

    float Toggle::floater_opacity_target() const {
        return checked ? 1.0f : 0.2f;
    }

    void Toggle::process_event(const Event &e) {
        switch (e.type) {
        case EventType::Click:
            if (is_enabled()) {
                set_checked_internal(!checked, true, false, true);
            }

            break;
        case EventType::Hover: {
            bool hover_active = std::get<EventHover>(e.variant).active && is_enabled();
            set_style_enabled(hover_state, hover_active);
            floater->set_style_enabled(hover_state, hover_active);
            break;
        }
        case EventType::Focus: {
            bool focus_active = std::get<EventFocus>(e.variant).active;
            set_style_enabled(focus_state, focus_active);
            break;
        }
        case EventType::Enable: {
            bool enable_active = std::get<EventEnable>(e.variant).active;
            set_style_enabled(disabled_state, !enable_active);
            floater->set_style_enabled(disabled_state, !enable_active);
            if (enable_active) {
                set_cursor(Cursor::Pointer);
                set_focusable(true);
            }
            else {
                set_cursor(Cursor::None);
                set_focusable(false);
            }
            break;
        }
        case EventType::Update: {
            std::chrono::high_resolution_clock::duration now = ultramodern::time_since_start();
            float delta_time = std::max(std::chrono::duration<float>(now - last_time).count(), 0.0f);
            last_time = now;

            constexpr float dp_speed = 740.0f;
            constexpr float opacity_speed = 8.0f;
            const float left_target = floater_left_target();
            const float opacity_target = floater_opacity_target();
            bool animating = move_towards(floater_left, left_target, dp_speed * delta_time);
            animating |= move_towards(floater_opacity, opacity_target, opacity_speed * delta_time);

            if (animating) {
                queue_update();
            }

            floater->set_left(floater_left, Unit::Dp);
            floater->set_opacity(floater_opacity);

            break;
        }
        default:
            break;
        }
    }

    void Toggle::set_checked(bool checked) {
        set_checked_internal(checked, false, false, false);
    }

    bool Toggle::is_checked() const {
        return checked;
    }

    void Toggle::add_checked_callback(std::function<void(bool)> callback) {
        checked_callbacks.emplace_back(callback);
    }
};
