#include <windows.h>

#include <cstdint>
#include <cstring>

#include "fh2mod.h"
#include "AutoshowLayout.h"

namespace {
// CarColorSelect is the native page backing A_CarDetails. Colour selection
// itself is a separate popup and receives input before this page.
constexpr std::uintptr_t kPageShown = 0xCA99A0;
constexpr std::uintptr_t kPageHidden = 0xCAD880;
constexpr std::uintptr_t kPageButton = 0xCEC3D0;
constexpr std::uintptr_t kGetUICar = 0xDA1560;
constexpr std::uintptr_t kUICarManager = 0x34E43B8;
constexpr std::uintptr_t kCarName = 0xF611B0;
constexpr std::uintptr_t kPlayCarState = 0xF5D300;
constexpr std::uintptr_t kPresentationVtable = 0x2EBE7E0;
constexpr char kDetailsFile[] = "UI/Scenes/Anthem/A_CarDetails.xaml";
constexpr int kYKey = 68;
constexpr unsigned kUICarSlot = 0x5A;

using PageLifecycle = void (*)(void*);
using PageButton = bool (*)(void*, int, bool);
PageLifecycle originalShown = nullptr;
PageLifecycle originalHidden = nullptr;
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

bool playPreviewState(bool open) {
    __try {
        if (!*reinterpret_cast<void**>(previewCar + 0x4D40))
            return false;
        auto play = reinterpret_cast<void (*)(void*, const char*)>(gameBase + kPlayCarState);
        play(previewCar, open ? "CarEvents:Explode" : "CarEvents:Implode");
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void leavePreview() {
    if (previewCar) {
        if (exploded)
            playPreviewState(false);
        if (void* presentation = forcedLod ? carPresentation() : nullptr)
            setLodConstraint(presentation, 0xFFFF);
        auto** vtable = *reinterpret_cast<void***>(previewCar);
        reinterpret_cast<void (*)(void*)>(vtable[0x18 / 8])(previewCar);
        previewCar = nullptr;
    }
    forcedLod = false;
    exploded = false;
    yDown = false;
    activePage = nullptr;
}

void shown(void* page) {
    originalShown(page);
    leavePreview();
    activePage = page;
    void* manager = *reinterpret_cast<void**>(gameBase + kUICarManager);
    if (!manager) {
        api->log(owner, "Autoshow details opened without a UI car manager.");
        return;
    }
    auto getCar = reinterpret_cast<void* (*)(void*, unsigned char**, unsigned)>(gameBase + kGetUICar);
    getCar(manager, &previewCar, kUICarSlot);
    if (!previewCar)
        api->log(owner, "Autoshow details opened without a preview car.");
    else
        api->log(owner, loadPreviewLod() ? "Autoshow details opened; requested preview LOD0."
                                       : "Autoshow details opened; could not request preview LOD0.");
}

void hidden(void* page) {
    if (page == activePage)
        leavePreview();
    originalHidden(page);
}

bool button(void* page, int key, bool pressed) {
    if (page != activePage || key != kYKey || !previewCar)
        return originalButton(page, key, pressed);
    if (pressed && !yDown) {
        if (playPreviewState(!exploded)) {
            exploded = !exploded;
            api->log(owner, exploded ? "Autoshow Y: requested Explode." : "Autoshow Y: requested Implode.");
        } else {
            api->log(owner, "Autoshow preview car animation is unavailable.");
        }
    }
    yDown = pressed;
    return true;
}
}

bool installAutoshowPrompt(const FH2ModApi* platform, FH2Mod* mod) {
    // Register before game startup so the UI can cache the modified layout.
    return platform->set_file &&
        platform->set_file(mod, kDetailsFile, kCarDetailsLayout, sizeof kCarDetailsLayout - 1);
}

void installAutoshowPreview(const FH2ModApi* platform, FH2Mod* mod, unsigned char* game) {
    constexpr unsigned char getCarCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10};
    constexpr unsigned char shownCode[] = {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24};
    constexpr unsigned char hiddenCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20};
    constexpr unsigned char buttonCode[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x40};
    if (!platform->hook_function) {
        platform->log(mod, "Autoshow preview requires the platform's function hooks.");
        return;
    }
    if (std::memcmp(game + kGetUICar, getCarCode, sizeof getCarCode) != 0 ||
        std::memcmp(game + kPageShown, shownCode, sizeof shownCode) != 0 ||
        std::memcmp(game + kPageHidden, hiddenCode, sizeof hiddenCode) != 0 ||
        std::memcmp(game + kPageButton, buttonCode, sizeof buttonCode) != 0) {
        platform->log(mod, "Autoshow preview could not verify the native details or car lookup function.");
        return;
    }
    gameBase = game;
    api = platform;
    owner = mod;
    FH2Hook* hooks[] = {
        platform->hook_function(mod, "Autoshow details shown", game + kPageShown,
            reinterpret_cast<void*>(shown), reinterpret_cast<void**>(&originalShown)),
        platform->hook_function(mod, "Autoshow details hidden", game + kPageHidden,
            reinterpret_cast<void*>(hidden), reinterpret_cast<void**>(&originalHidden)),
        platform->hook_function(mod, "Autoshow details buttons", game + kPageButton,
            reinterpret_cast<void*>(button), reinterpret_cast<void**>(&originalButton)),
    };
    bool installed = true;
    for (FH2Hook* hook : hooks)
        installed = installed && hook && platform->enable_hooks(mod, hook);
    if (!installed) {
        for (FH2Hook* hook : hooks)
            if (hook)
                platform->disable_hooks(mod, hook);
        platform->log(mod, "Could not enable the Autoshow preview hooks.");
        return;
    }
    platform->log(mod, "Autoshow preview enabled. Y toggles Explode and Implode.");
}
