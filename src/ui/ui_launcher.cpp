#include "recomp_ui.h"
#include "zelda_support.h"
#include "librecomp/game.hpp"
#include "ultramodern/ultramodern.hpp"
#include "RmlUi/Core.h"
#include "nfd.h"
#include "../../lib/rt64/src/contrib/stb/stb_image.h"
#include <filesystem>
#include <fstream>
#include <iterator>

static std::string version_string;

Rml::DataModelHandle model_handle;
bool mm_rom_valid = false;
static constexpr const char* launcher_background_src = "?/launcher/background";
static constexpr const char* launcher_logo_src = "?/launcher/logo";
static constexpr const char* launcher_version_rock_src = "?/launcher/version_rock";

extern std::vector<recomp::GameEntry> supported_games;

void select_rom() {
    nfdnchar_t* native_path = nullptr;
    zelda64::open_file_dialog([](bool success, const std::filesystem::path& path) {
        if (success) {
            recomp::RomValidationError rom_error = recomp::select_rom(path, supported_games[0].game_id);
            switch (rom_error) {
                case recomp::RomValidationError::Good:
                    mm_rom_valid = true;
                    model_handle.DirtyVariable("mm_rom_valid");
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
                    recompui::message_box(
                            "This ROM is the correct game, but the wrong version.\nThis project requires the NTSC-U N64 version of the game.");
                    break;
                case recomp::RomValidationError::OtherError:
                    recompui::message_box("An unknown error has occurred.");
                    break;
            }
        }
    });
}

recompui::ContextId launcher_context;

recompui::ContextId recompui::get_launcher_context_id() {
	return launcher_context;
}

static void load_launcher_background_image() {
    std::filesystem::path background_path = zelda64::get_asset_path("launcher_background.png");
    std::ifstream background_file{background_path, std::ios::binary};
    if (!background_file) {
        return;
    }

    std::vector<char> background_bytes{
        std::istreambuf_iterator<char>{background_file},
        std::istreambuf_iterator<char>{}
    };

    int width = 0;
    int height = 0;
    stbi_uc* background_rgba = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(background_bytes.data()),
        static_cast<int>(background_bytes.size()),
        &width,
        &height,
        nullptr,
        4
    );

    if (background_rgba == nullptr || width <= 0 || height <= 0) {
        if (background_rgba != nullptr) {
            stbi_image_free(background_rgba);
        }
        return;
    }

    std::vector<char> rgba_bytes{
        reinterpret_cast<char*>(background_rgba),
        reinterpret_cast<char*>(background_rgba) + (width * height * 4)
    };
    stbi_image_free(background_rgba);

    recompui::release_image(launcher_background_src);
    recompui::queue_image_from_bytes_rgba32(launcher_background_src, rgba_bytes, width, height);
}

static void load_launcher_logo_image() {
    std::filesystem::path logo_path = zelda64::get_asset_path("logo.png");
    std::ifstream logo_file{logo_path, std::ios::binary};
    if (!logo_file) {
        return;
    }

    std::vector<char> logo_bytes{
        std::istreambuf_iterator<char>{logo_file},
        std::istreambuf_iterator<char>{}
    };

    recompui::release_image(launcher_logo_src);
    recompui::queue_image_from_bytes_file(launcher_logo_src, logo_bytes);
}

static void load_launcher_version_rock_image() {
    std::filesystem::path rock_path = zelda64::get_asset_path("rock.png");
    std::ifstream rock_file{rock_path, std::ios::binary};
    if (!rock_file) {
        return;
    }

    std::vector<char> rock_bytes{
        std::istreambuf_iterator<char>{rock_file},
        std::istreambuf_iterator<char>{}
    };

    recompui::release_image(launcher_version_rock_src);
    recompui::queue_image_from_bytes_file(launcher_version_rock_src, rock_bytes);
}

class LauncherMenu : public recompui::MenuController {
public:
    LauncherMenu() {
        mm_rom_valid = recomp::is_rom_valid(supported_games[0].game_id);
    }
    ~LauncherMenu() override {

    }
    void load_document() override {
        load_launcher_background_image();
        load_launcher_logo_image();
        load_launcher_version_rock_image();
		launcher_context = recompui::create_context(zelda64::get_asset_path("launcher.rml"));
    }
    void register_events(recompui::UiEventListenerInstancer& listener) override {
        recompui::register_event(listener, "select_rom",
            [](const std::string& param, Rml::Event& event) {
                select_rom();
            }
        );
        recompui::register_event(listener, "rom_selected",
            [](const std::string& param, Rml::Event& event) {
                mm_rom_valid = true;
                model_handle.DirtyVariable("mm_rom_valid");
            }
        );
        recompui::register_event(listener, "start_game",
            [](const std::string& param, Rml::Event& event) {
                recomp::start_game(supported_games[0].game_id);
                recompui::hide_all_contexts();
            }
        );
        recompui::register_event(listener, "open_controls",
            [](const std::string& param, Rml::Event& event) {
                recompui::set_config_tab(recompui::ConfigTab::Controls);
                recompui::set_config_menu_uses_launcher_background(true);
                recompui::hide_all_contexts();
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        );
        recompui::register_event(listener, "open_settings",
            [](const std::string& param, Rml::Event& event) {
                recompui::set_config_tab(recompui::ConfigTab::General);
                recompui::set_config_menu_uses_launcher_background(true);
                recompui::hide_all_contexts();
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        );
        recompui::register_event(listener, "open_mods",
            [](const std::string &param, Rml::Event &event) {
                recompui::set_config_tab(recompui::ConfigTab::Mods);
                recompui::set_config_menu_uses_launcher_background(true);
                recompui::hide_all_contexts();
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        );
        recompui::register_event(listener, "exit_game",
            [](const std::string& param, Rml::Event& event) {
                ultramodern::quit();
            }
        );
    }
    void make_bindings(Rml::Context* context) override {
        Rml::DataModelConstructor constructor = context->CreateDataModel("launcher_model");

        constructor.Bind("mm_rom_valid", &mm_rom_valid);

        version_string = recomp::get_project_version().to_string();
        constructor.Bind("version_number", &version_string);

        model_handle = constructor.GetModelHandle();
    }
};

std::unique_ptr<recompui::MenuController> recompui::create_launcher_menu() {
    return std::make_unique<LauncherMenu>();
}
