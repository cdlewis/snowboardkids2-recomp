#include "sk2_launcher.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "recompui/recompui.h"

#include "base/ui_launcher.h"
#include "core/ui_context.h"
#include "elements/ui_element.h"
#include "elements/ui_image.h"
#include "elements/ui_label.h"
#include "elements/ui_svg.h"
#include "librecomp/game.hpp"
#include "recompui/config.h"
#include "ultramodern/ultramodern.hpp"
#include "util/file.h"

#include "../../lib/rt64/src/contrib/stb/stb_image.h"

namespace {

class Sk2LauncherOption;
Sk2LauncherOption* start_option = nullptr;

class Sk2LauncherOption : public recompui::Element {
  public:
    Sk2LauncherOption(recompui::ResourceId rid, recompui::Element* parent, const std::string& title,
                      std::function<void()> callback, bool primary)
        : Element(rid, parent,
                  recompui::Events(recompui::EventType::Click, recompui::EventType::Hover, recompui::EventType::Focus,
                                   recompui::EventType::Enable),
                  "button", false),
          callback(std::move(callback)), primary_option(primary) {
        using namespace recompui;

        set_display(Display::Flex);
        set_position(Position::Relative);
        set_align_items(AlignItems::Center);
        set_justify_content(JustifyContent::Center);
        set_width(70.0f, Unit::Percent);
        set_height(78.0f);
        set_margin_bottom(6.0f);
        set_padding(0.0f);
        set_border_width(0.0f);
        set_border_radius(8.0f);
        set_background_color(Color{ 0, 0, 0, 0 });
        set_color(Color{ 16, 129, 224, 255 });
        set_cursor(Cursor::Pointer);
        set_focusable(true);
        set_tab_index_auto();
        if (primary) {
            set_as_primary_focus(true);
            initial_selection_retained = true;
        }

        auto context = get_current_context();
        inactive_board = context.create_element<Svg>(this, "board.svg");
        style_board(inactive_board);
        inactive_board->set_opacity(primary ? 0.0f : 1.0f);

        active_board = context.create_element<Svg>(this, "board-selected.svg");
        style_board(active_board);
        active_board->set_opacity(primary ? 1.0f : 0.0f);

        label = context.create_element<Label>(this, title, recompui::theme::Typography::LabelLG);
        label->set_position(Position::Relative);
        label->set_width(100.0f, Unit::Percent);
        label->set_padding_bottom(10.0f);
        label->set_text_align(TextAlign::Center);
        label->set_font_family("Fredoka");
        label->set_font_weight(600);
        label->set_font_size(36.0f);
        label->set_letter_spacing(3.96f);
        label->set_line_height(36.0f);
        label->set_color(primary ? Color{ 255, 255, 255, 255 } : Color{ 16, 129, 224, 255 });
    }

    void set_title(const std::string& title) {
        label->set_text(title);
    }

  protected:
    std::string_view get_type_name() override {
        return "Sk2LauncherOption";
    }

    void process_event(const recompui::Event& e) override {
        using namespace recompui;

        switch (e.type) {
            case EventType::Click:
                if (is_enabled() && callback) {
                    callback();
                }
                break;
            case EventType::Hover:
                hovered = std::get<EventHover>(e.variant).active;
                if (hovered && !primary_option && start_option != nullptr) {
                    start_option->release_initial_selection();
                }
                update_state();
                break;
            case EventType::Focus:
                focused = std::get<EventFocus>(e.variant).active;
                if (focused && !primary_option && start_option != nullptr) {
                    start_option->release_initial_selection();
                }
                update_state();
                break;
            case EventType::Enable:
                enabled = std::get<EventEnable>(e.variant).active;
                set_cursor(enabled ? Cursor::Pointer : Cursor::None);
                set_focusable(enabled);
                update_state();
                break;
            default:
                break;
        }
    }

  private:
    std::function<void()> callback;
    recompui::Svg* inactive_board = nullptr;
    recompui::Svg* active_board = nullptr;
    recompui::Label* label = nullptr;
    bool primary_option = false;
    bool hovered = false;
    bool focused = false;
    bool enabled = true;
    bool initial_selection_retained = false;

  public:
    void release_initial_selection() {
        initial_selection_retained = false;
        update_state();
    }

  private:

    static void style_board(recompui::Svg* board) {
        using namespace recompui;

        board->set_position(Position::Absolute);
        board->set_inset(0.0f);
        board->set_width(100.0f, Unit::Percent);
        board->set_height(100.0f, Unit::Percent);
        board->set_pointer_events(PointerEvents::None);
    }

    void update_state() {
        bool active = enabled && (hovered || focused || initial_selection_retained);
        inactive_board->set_opacity(active ? 0.0f : 1.0f);
        active_board->set_opacity(active ? 1.0f : 0.0f);
        label->set_color(active ? recompui::Color{ 255, 255, 255, 255 } : recompui::Color{ 16, 129, 224, 255 });
        set_opacity(enabled ? 1.0f : 0.5f);

        auto transform = Rml::MakeShared<Rml::Transform>();
        transform->AddPrimitive(Rml::Transforms::Scale2D(active ? 1.05f : 1.0f, active ? 1.05f : 1.0f));
        animate_property(Rml::PropertyId::Transform,
                         Rml::Property(Rml::Variant(std::move(transform)), Rml::Unit::TRANSFORM), 0.18f,
                         Rml::Tween{ Rml::Tween::Back, Rml::Tween::Out });
    }
};

bool rom_valid = false;
bool initial_focus_pending = false;
std::u8string game_id;
std::string mod_game_id;

constexpr const char* launcher_background_src = "?/sk2_launcher/background";
constexpr const char* launcher_logo_src = "?/sk2_launcher/logo";
constexpr const char* launcher_rock_src = "?/sk2_launcher/rock";

std::vector<char> read_asset_bytes(const char* asset_name) {
    std::filesystem::path path = recompui::file::get_asset_path(asset_name);
    std::ifstream file{ path, std::ios::binary };
    if (!file) {
        return {};
    }

    return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
}

void queue_launcher_png_assets() {
    std::vector<char> background_bytes = read_asset_bytes("launcher_background.png");
    if (!background_bytes.empty()) {
        int width = 0;
        int height = 0;
        stbi_uc* background_rgba =
            stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(background_bytes.data()),
                                  static_cast<int>(background_bytes.size()), &width, &height, nullptr, 4);

        if (background_rgba != nullptr && width > 0 && height > 0) {
            std::vector<char> rgba_bytes{ reinterpret_cast<char*>(background_rgba),
                                          reinterpret_cast<char*>(background_rgba) + (width * height * 4) };
            recompui::release_image(launcher_background_src);
            recompui::queue_image_from_bytes_rgba32(launcher_background_src, rgba_bytes, width, height);
        }

        if (background_rgba != nullptr) {
            stbi_image_free(background_rgba);
        }
    }

    std::vector<char> logo_bytes = read_asset_bytes("logo.png");
    if (!logo_bytes.empty()) {
        recompui::release_image(launcher_logo_src);
        recompui::queue_image_from_bytes_file(launcher_logo_src, logo_bytes);
    }

    std::vector<char> rock_bytes = read_asset_bytes("rock.png");
    if (!rock_bytes.empty()) {
        recompui::release_image(launcher_rock_src);
        recompui::queue_image_from_bytes_file(launcher_rock_src, rock_bytes);
    }
}

void open_config_tab(std::string_view tab_id) {
    recompui::update_game_mod_id(mod_game_id);
    recompui::config::set_tab(std::string{ tab_id });
    recompui::config::open();
}

void start_or_select_rom() {
    if (rom_valid) {
        recompui::update_game_mod_id(mod_game_id);
        recomp::start_game(game_id, {});
        recompui::hide_all_contexts();
        return;
    }

    recompui::file::open_file_dialog([](bool success, const std::filesystem::path& path) {
        if (!success) {
            return;
        }

        recomp::RomValidationError rom_error = recomp::select_rom(path, game_id);
        switch (rom_error) {
            case recomp::RomValidationError::Good:
                rom_valid = true;
                if (start_option != nullptr) {
                    start_option->set_title("Start game");
                }
                break;
            case recomp::RomValidationError::FailedToOpen:
                recompui::message_box("Failed to open ROM file.");
                break;
            case recomp::RomValidationError::NotARom:
                recompui::message_box("This is not a valid ROM file.");
                break;
            case recomp::RomValidationError::IncorrectRom:
                recompui::message_box("This ROM is not the correct game.");
                break;
            case recomp::RomValidationError::NotYet:
                recompui::message_box("This game isn't supported yet.");
                break;
            case recomp::RomValidationError::IncorrectVersion:
                recompui::message_box("This ROM is the correct game, but the wrong version.\n"
                                      "This project requires the NTSC-U N64 version of the game.");
                break;
            case recomp::RomValidationError::OtherError:
                recompui::message_box("An unknown error has occurred.");
                break;
        }
    });
}

void on_launcher_init(recompui::LauncherMenu* menu) {
    using namespace recompui;

    rom_valid = recomp::is_rom_valid(game_id);
    start_option = nullptr;

    auto context = get_current_context();
    queue_launcher_png_assets();
    menu->remove_default_title();

    menu->get_background_container()->clear_children();
    menu->get_menu_container()->clear_children();

    auto* background = context.create_element<Image>(menu, launcher_background_src);
    background->set_position(Position::Absolute);
    background->set_inset(0.0f);
    background->set_width(100.0f, Unit::Percent);
    background->set_height(100.0f, Unit::Percent);
    background->set_pointer_events(PointerEvents::None);

    auto* logo = context.create_element<Image>(menu, launcher_logo_src);
    logo->set_position(Position::Absolute);
    logo->set_top(20.0f);
    logo->set_left(30.0f);
    logo->set_width(620.0f);
    logo->set_height(249.0f);
    logo->set_pointer_events(PointerEvents::None);

    auto* version_group = context.create_element<Element>(menu);
    version_group->set_display(Display::Flex);
    version_group->set_position(Position::Absolute);
    version_group->set_left(27.0f);
    version_group->set_bottom(20.0f);
    version_group->set_width(220.0f);
    version_group->set_height(112.0f);
    version_group->set_align_items(AlignItems::Center);
    version_group->set_justify_content(JustifyContent::Center);

    auto* rock = context.create_element<Image>(version_group, launcher_rock_src);
    rock->set_position(Position::Absolute);
    rock->set_top(3.0f);
    rock->set_left(0.0f);
    rock->set_width(100.0f, Unit::Percent);
    rock->set_height(100.0f, Unit::Percent);
    rock->set_pointer_events(PointerEvents::None);

    auto* version = context.create_element<Label>(version_group, "v" + recomp::get_project_version().to_string(),
                                                  LabelStyle::Small);
    version->set_position(Position::Relative);
    version->set_top(-2.0f);
    version->set_left(-2.0f);
    version->set_font_family("Fredoka");
    version->set_font_size(27.0f);
    version->set_font_weight(400);
    version->set_line_height(27.0f);
    version->set_color(Color{ 255, 255, 255, 230 });

    Element* menu_container = context.create_element<Element>(menu);
    menu_container->set_position(Position::Absolute);
    menu_container->set_top(0.0f);
    menu_container->set_right(0.0f);
    menu_container->set_bottom(0.0f);
    menu_container->set_width(650.0f);
    menu_container->set_height(100.0f, Unit::Percent);
    menu_container->set_padding_right(40.0f);
    menu_container->set_padding_bottom(34.0f);
    menu_container->set_display(Display::Flex);
    menu_container->set_flex_direction(FlexDirection::Column);
    menu_container->set_align_items(AlignItems::FlexEnd);
    menu_container->set_justify_content(JustifyContent::FlexEnd);
    menu_container->set_as_navigation_container(NavigationType::Vertical);
    menu_container->set_nav_wrapping(true);

    start_option = context.create_element<Sk2LauncherOption>(menu_container, rom_valid ? "Start game" : "Select ROM",
                                                             start_or_select_rom, true);
    context.set_autofocus_element(start_option);
    initial_focus_pending = true;

    context.create_element<Sk2LauncherOption>(
        menu_container, "Controls", []() { open_config_tab(recompui::config::controls::id); }, false);
    context.create_element<Sk2LauncherOption>(
        menu_container, "Settings", []() { open_config_tab(recompui::config::general::id); }, false);
    context.create_element<Sk2LauncherOption>(
        menu_container, "Mods", []() { open_config_tab(recompui::config::mods::id); }, false);
    context.create_element<Sk2LauncherOption>(menu_container, "Exit", []() { ultramodern::quit(); }, false);
}

void on_launcher_update(recompui::LauncherMenu*) {
    if (initial_focus_pending && start_option != nullptr) {
        start_option->focus();
        initial_focus_pending = false;
    }
}

} // namespace

namespace sk2::launcher {

void register_callbacks(const recomp::GameEntry& game) {
    game_id = game.game_id;
    mod_game_id = game.mod_game_id;

    recompui::register_launcher_init_callback(on_launcher_init);
    recompui::register_launcher_update_callback(on_launcher_update);
    recompui::config::set_modal_on_close_callback([]() {
        if (start_option != nullptr) {
            start_option->blur();
        }
    });
}

} // namespace sk2::launcher
