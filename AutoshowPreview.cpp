#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "fh2mod.h"
#include "AutoshowLayout.h"

namespace {
// CarColorSelect is the native page backing A_CarDetails. Colour selection
// itself is a separate popup and receives input before this page.
constexpr std::uintptr_t kPageShown = 0xCA99A0;
constexpr std::uintptr_t kPageHidden = 0xCAD880;
constexpr std::uintptr_t kPageButton = 0xCEC3D0;
constexpr std::uintptr_t kGarageShown = 0xCCE280;
constexpr std::uintptr_t kGaragePauseShown = 0xCF8D80;
constexpr std::uintptr_t kGaragePauseVtable = 0x2D63870;
constexpr std::uintptr_t kGarageHidden = 0x14198F0;
constexpr std::uintptr_t kGetUICar = 0xDA1560;
constexpr std::uintptr_t kUICarManager = 0x34E43B8;
constexpr std::uintptr_t kCarName = 0xF611B0;
constexpr std::uintptr_t kPlayCarState = 0xF5D300;
constexpr std::uintptr_t kPresentationVtable = 0x2EBE7E0;
constexpr char kDetailsFile[] = "UI/Scenes/Anthem/A_CarDetails.xaml";
constexpr char kGarageFile[] = "UI/Scenes/Anthem/A_ViewCar.xaml";
constexpr int kYKey = 68;
constexpr int kBackKey = 45;
constexpr unsigned kUICarSlot = 0x5A;

using PageLifecycle = void (*)(void*);
using PageButton = bool (*)(void*, int, bool);
PageLifecycle originalShown = nullptr;
PageLifecycle originalHidden = nullptr;
PageLifecycle originalGarageShown = nullptr;
PageLifecycle originalGaragePauseShown = nullptr;
PageLifecycle originalGarageHidden = nullptr;
PageButton originalButton = nullptr;
unsigned char* gameBase = nullptr;
const FH2ModApi* api = nullptr;
FH2Mod* owner = nullptr;

// The UI thread owns these. GetUICar returns an intrusive reference.
void* activePage = nullptr;
unsigned char* previewCar = nullptr;
bool forcedLod = false;
bool exploded = false;
bool yDown = false;
bool garagePreview = false;

void setLodConstraint(void* presentation, std::uint16_t mask) {
    auto** vtable = *static_cast<void***>(presentation);
    reinterpret_cast<void (*)(void*, std::uint16_t)>(vtable[0x180 / 8])(presentation, mask);
}

void* carPresentation() {
    void* presentation = *reinterpret_cast<void**>(previewCar + 0x4D38);
    return presentation && *static_cast<void**>(presentation) == gameBase + kPresentationVtable
        ? presentation : nullptr;
}

bool loadPreviewLod() {
    __try {
        void* presentation = carPresentation();
        if (!presentation)
            return false;
        // The game uses this sequence in LoadLOD0ForCarSelect.
        auto** vtable = *static_cast<void***>(presentation);
        setLodConstraint(presentation, 2);
        forcedLod = true;
        // The getter constructs a 0x68-byte resource descriptor. The
        // deferred LoadLOD0 method consumes and destroys that descriptor.
        alignas(8) unsigned char carResource[0x68]{};
        reinterpret_cast<void* (*)(void*, void*)>(gameBase + kCarName)(previewCar, carResource);
        reinterpret_cast<void (*)(void*, void*)>(vtable[0x3B0 / 8])(presentation, carResource);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool playPreviewState(unsigned char* car, bool open) {
    __try {
        if (!car || !*reinterpret_cast<void**>(car + 0x4D40))
            return false;
        auto play = reinterpret_cast<void (*)(void*, const char*)>(gameBase + kPlayCarState);
        play(car, open ? "CarEvents:Explode" : "CarEvents:Implode");
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

unsigned char* acquireUICar() {
    unsigned char* car = nullptr;
    if (void* manager = *reinterpret_cast<void**>(gameBase + kUICarManager)) {
        auto getCar = reinterpret_cast<void* (*)(void*, unsigned char**, unsigned)>(gameBase + kGetUICar);
        getCar(manager, &car, kUICarSlot);
    }
    return car;
}

void releaseUICar(unsigned char* car) {
    auto** vtable = *reinterpret_cast<void***>(car);
    reinterpret_cast<void (*)(void*)>(vtable[0x18 / 8])(car);
}

bool playGarageState(bool open) {
    // Garage's native Back flow can unload the selected UI car before the
    // page is hidden. Hold the reference only for the animation call.
    auto* car = acquireUICar();
    if (!car)
        return false;
    const bool played = playPreviewState(car, open);
    releaseUICar(car);
    return played;
}

void leavePreview() {
    if (previewCar) {
        if (exploded)
            playPreviewState(previewCar, false);
        if (void* presentation = forcedLod ? carPresentation() : nullptr)
            setLodConstraint(presentation, 0xFFFF);
        releaseUICar(previewCar);
        previewCar = nullptr;
    }
    forcedLod = false;
    exploded = false;
    yDown = false;
    activePage = nullptr;
    garagePreview = false;
}

void enterPreview(void* page, bool garage = false) {
    leavePreview();
    activePage = page;
    garagePreview = garage;
    if (garage)
        return;
    previewCar = acquireUICar();
    if (!previewCar)
        api->log(owner, "Car preview opened without a UI car.");
    else
        api->log(owner, loadPreviewLod() ? "Car preview opened; requested preview LOD0."
                                       : "Car preview opened; could not request preview LOD0.");
}

void shown(void* page) {
    originalShown(page);
    enterPreview(page);
}

void garageShown(void* page) {
    originalGarageShown(page);
    enterPreview(page, true);
}

void garagePauseShown(void* page) {
    originalGaragePauseShown(page);
    // Several pages share this method. Only the pause Garage's View Car
    // screen should acquire its UI car and handle Y.
    if (*static_cast<void**>(page) == gameBase + kGaragePauseVtable) {
        api->log(owner, "Pause Garage View Car shown.");
        enterPreview(page, true);
    }
}

void hidden(void* page) {
    if (page == activePage)
        leavePreview();
    originalHidden(page);
}

void garageHidden(void* page) {
    if (page == activePage)
        leavePreview();
    originalGarageHidden(page);
}

bool button(void* page, int key, bool pressed) {
    if (page != activePage)
        return originalButton(page, key, pressed);
    if (garagePreview && key == kBackKey && pressed && exploded) {
        playGarageState(false);
        exploded = false;
    }
    if (key != kYKey || (!garagePreview && !previewCar))
        return originalButton(page, key, pressed);
    if (pressed && !yDown) {
        const bool played = garagePreview ? playGarageState(!exploded) : playPreviewState(previewCar, !exploded);
        if (played) {
            exploded = !exploded;
            api->log(owner, exploded ? "Car preview Y: requested Explode." : "Car preview Y: requested Implode.");
        } else {
            api->log(owner, "Car preview animation is unavailable.");
        }
    }
    yDown = pressed;
    return true;
}
}

void installPreviewPrompts(const FH2ModApi* platform, FH2Mod* mod, bool autoshow) {
    // Register before game startup so the UI can cache the modified layout.
    if (autoshow && (!platform->set_file ||
        !platform->set_file(mod, kDetailsFile, kCarDetailsLayout, sizeof kCarDetailsLayout - 1)))
        platform->log(mod, "Could not register the Autoshow Y prompt.");
    if (!platform->set_file ||
        !platform->set_file(mod, kGarageFile, kViewCarLayout, sizeof kViewCarLayout - 1))
        platform->log(mod, "Could not register the Garage Y prompt.");

    if (platform->api_version < 10 || !platform->add_translation || !platform->set_string_tr)
        return;
    constexpr struct { const char* language; const char* label; } labels[] = {
        {"zh-Hant", u8"\u5C55\u5F00"}, {"cs", u8"Rozlo\u017E"}, {"es-ES", "Exponer todo"},
        {"es-MX", "Abrir todo"}, {"hu", u8"Kinyit\u00E1s"},
        {"pt-BR", "Explodir"}, {"de", u8"\u00D6ffnen"}, {"fr", "Exploser"},
        {"it", "Apri tutto"}, {"ja", u8"\u3059\u3079\u3066\u958B\u3051\u308B"},
        {"ko", u8"\uBAA8\uB450 \uC5F4\uAE30"}, {"nl", "Exploderen"}, {"pl", "Eksplozja"},
        {"ru", u8"\u041E\u0434\u043D\u043E\u0432\u0440\u0435\u043C\u0435\u043D\u043D\u043E\u0435 \u043E\u0442\u043A\u0440\u044B\u0432\u0430\u043D\u0438\u0435 \u0434\u0432\u0435\u0440\u0435\u0439"},
    };
    for (const auto& label : labels) {
        const std::string po = std::string("msgctxt \"preview.explode\"\nmsgid \"Explode\"\nmsgstr \"") +
            label.label + "\"\n";
        if (!platform->add_translation(mod, label.language, po.data(), po.size()))
            platform->log(mod, "Could not register an Explode translation.");
    }
    platform->set_string_tr(mod, "HelpButtons.str", 0x05FF, "preview.explode", L"Explode");
}

void installCarPreviews(const FH2ModApi* platform, FH2Mod* mod, unsigned char* game, bool autoshow) {
    constexpr unsigned char getCarCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10};
    constexpr unsigned char shownCode[] = {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24};
    constexpr unsigned char hiddenCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20};
    constexpr unsigned char buttonCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x40};
    constexpr unsigned char garageShownCode[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x28};
    constexpr unsigned char garagePauseShownCode[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x30};
    constexpr unsigned char garageHiddenCode[] = {0xC6, 0x81, 0x0C, 0x03, 0x00, 0x00, 0x00};
    if (!platform->hook_function) {
        platform->log(mod, "Autoshow preview requires the platform's function hooks.");
        return;
    }
    if (std::memcmp(game + kGetUICar, getCarCode, sizeof getCarCode) != 0 ||
        (autoshow && (std::memcmp(game + kPageShown, shownCode, sizeof shownCode) != 0 ||
                      std::memcmp(game + kPageHidden, hiddenCode, sizeof hiddenCode) != 0)) ||
        std::memcmp(game + kGarageShown, garageShownCode, sizeof garageShownCode) != 0 ||
        std::memcmp(game + kGaragePauseShown, garagePauseShownCode, sizeof garagePauseShownCode) != 0 ||
        std::memcmp(game + kGarageHidden, garageHiddenCode, sizeof garageHiddenCode) != 0 ||
        std::memcmp(game + kPageButton, buttonCode, sizeof buttonCode) != 0) {
        platform->log(mod, "Autoshow preview could not verify the native details or car lookup function.");
        return;
    }
    gameBase = game;
    api = platform;
    owner = mod;
    FH2Hook* hooks[] = {
        platform->hook_function(mod, "Garage view shown", game + kGarageShown,
            reinterpret_cast<void*>(garageShown), reinterpret_cast<void**>(&originalGarageShown)),
        platform->hook_function(mod, "Pause Garage view shown", game + kGaragePauseShown,
            reinterpret_cast<void*>(garagePauseShown), reinterpret_cast<void**>(&originalGaragePauseShown)),
        platform->hook_function(mod, "Garage view hidden", game + kGarageHidden,
            reinterpret_cast<void*>(garageHidden), reinterpret_cast<void**>(&originalGarageHidden)),
        platform->hook_function(mod, "Car preview buttons", game + kPageButton,
            reinterpret_cast<void*>(button), reinterpret_cast<void**>(&originalButton)),
        autoshow ? platform->hook_function(mod, "Autoshow details shown", game + kPageShown,
            reinterpret_cast<void*>(shown), reinterpret_cast<void**>(&originalShown)) : nullptr,
        autoshow ? platform->hook_function(mod, "Autoshow details hidden", game + kPageHidden,
            reinterpret_cast<void*>(hidden), reinterpret_cast<void**>(&originalHidden)) : nullptr,
    };
    bool installed = true;
    for (unsigned i = 0; i < (autoshow ? 6u : 4u); ++i)
        installed = installed && hooks[i] && platform->enable_hooks(mod, hooks[i]);
    if (!installed) {
        for (FH2Hook* hook : hooks)
            if (hook)
                platform->disable_hooks(mod, hook);
        platform->log(mod, "Could not enable the car preview hooks.");
        return;
    }
    platform->log(mod, "Car previews enabled. Y toggles Explode and Implode.");
}
