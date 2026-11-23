#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "fh2mod.h"

namespace {
constexpr std::uintptr_t kParseModelRva = 0x13F1D70;
constexpr std::uintptr_t kApplyRenderModeRva = 0xF908D0;
constexpr std::uintptr_t kReadAutovistaRva = 0x130AAE0;
constexpr std::uintptr_t kAutovistaControlVtableRva = 0x2EB9C38;
constexpr std::uintptr_t kReadSceneModelRva = 0x11ADEE0;
constexpr std::uintptr_t kReadSceneRva = 0x11AECB0;
constexpr unsigned char kReadSceneCode[] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x48, 0x89, 0x48, 0x08
};
constexpr unsigned char kReadSceneModelCode[] = {
    0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC
};
constexpr unsigned char kParseModelCode[] = {
    0x4C, 0x89, 0x44, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55
};
constexpr unsigned char kApplyRenderModeCode[] = {
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC
};
constexpr unsigned char kReadAutovistaCode[] = {
    0x48, 0x8B, 0x02, 0x4C, 0x8B, 0xCA, 0x48, 0x8D, 0x51, 0x08
};
constexpr std::uint16_t kShowroom = 1;
constexpr std::uint16_t kLod0 = 2;
constexpr std::string_view kCarsRoot = "game:\\media\\cars\\";
constexpr std::string_view kShowroomSuffix = "__slod.modelbin";

using ParseModel = bool (*)(void*, void*, const void*);
using ApplyRenderMode = void (*)(void*);
using ReadControl = std::uintptr_t (*)(void*, void*);
using ReadSceneModel = std::uintptr_t (*)(void*, void*, void*);
ParseModel originalParseModel = nullptr;
ApplyRenderMode originalApplyRenderMode = nullptr;
ReadControl originalReadControl = nullptr;
ReadSceneModel originalReadSceneModel = nullptr;
ReadSceneModel originalReadScene = nullptr;
const void* autovistaControlVtable = nullptr;
const FH2ModApi* platform = nullptr;
FH2Mod* mod = nullptr;
std::atomic<unsigned> modelMessages{0};
std::atomic<unsigned> controlMessages{0};
std::atomic<unsigned> sceneMessages{0};
std::mutex pairedPartMutex;
std::unordered_set<std::string> pairedPartAssets;

struct ScenePart {
    unsigned char* record;
    std::string path;
    std::uint16_t originalMask;
};
thread_local std::vector<ScenePart>* sceneParts = nullptr;

bool copyAssetPath(const unsigned char* path, char* output, std::size_t capacity) {
    if (!path)
        return false;
    __try {
        const auto length = *reinterpret_cast<const std::size_t*>(path + 0x10);
        const auto allocation = *reinterpret_cast<const std::size_t*>(path + 0x18);
        if (!length || length >= capacity || length > allocation || allocation < 15)
            return false;
        const char* text = allocation < 16 ? reinterpret_cast<const char*>(path)
                                           : *reinterpret_cast<const char* const*>(path);
        for (std::size_t index = 0; index < length; ++index) {
            const auto value = static_cast<unsigned char>(text[index]);
            if (value < 0x20 || value > 0x7E)
                return false;
            char lower = value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
            output[index] = lower == '/' ? '\\' : lower;
        }
        output[length] = '\0';
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void logMask(const char* label, const char* path, std::uint16_t mask) {
    char message[640];
    std::snprintf(message, sizeof message, "%s 0x%04X: %s",
                  label, static_cast<unsigned>(mask), path);
    platform->log(mod, message);
}

bool isCarModel(std::string_view path) {
    return path.rfind(kCarsRoot, 0) == 0 && path.size() > kCarsRoot.size() &&
           path.size() >= 9 && path.substr(path.size() - 9) == ".modelbin";
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool supportsMeshSuppression(std::string_view path) {
    const auto name = path.substr(path.find_last_of('\\') + 1);
    return name == "engine_a.modelbin" || name.rfind("enginebay_", 0) == 0 || name.rfind("trunkbay_", 0) == 0 ||
           name.rfind("trunkliner_", 0) == 0;
}

bool sameAttachment(const ScenePart& a, const ScenePart& b) {
    // Both model records carry the attachment matrix and the resolved bone ID.
    return std::memcmp(a.record + 0x1E0, b.record + 0x1E0, 64) == 0 &&
           std::memcmp(a.record + 0x220, b.record + 0x220, sizeof(std::uint16_t)) == 0;
}

bool isReading(void* reader) {
    using IsReading = bool (*)(void*);
    const auto* vtable = *static_cast<void***>(reader);
    return reinterpret_cast<IsReading>(vtable[0xB8 / sizeof(void*)])(reader);
}

std::uintptr_t readScene(void* scene, void* reader, void* context) {
    if (!isReading(reader))
        return originalReadScene(scene, reader, context);
    std::vector<ScenePart> parts;
    auto* previous = sceneParts;
    sceneParts = &parts;
    const auto result = originalReadScene(scene, reader, context);
    sceneParts = previous;

    for (const auto& showroom : parts) {
        if (showroom.originalMask != kShowroom || !endsWith(showroom.path, kShowroomSuffix))
            continue;
        std::string stockPath = showroom.path.substr(0, showroom.path.size() - kShowroomSuffix.size());
        stockPath += ".modelbin";
        const ScenePart* replacement = nullptr;
        for (const auto& stock : parts) {
            if (stock.path != stockPath || !(stock.originalMask & kLod0) || !sameAttachment(showroom, stock))
                continue;
            if (replacement) {
                replacement = nullptr;
                break;
            }
            replacement = &stock;
        }
        if (replacement) {
            const std::string_view stockName = std::string_view(replacement->path).substr(
                replacement->path.find_last_of('\\') + 1);
            // A hood or liner already present in the native showroom layer
            // can share that layer with a separate strut or hinge model.
            if ((replacement->originalMask & kShowroom) &&
                (stockName.rfind("hood_", 0) == 0 || stockName.rfind("hoodliner_", 0) == 0))
                continue;

            auto& mask = *reinterpret_cast<std::uint16_t*>(replacement->record + 0x1D2);
            mask &= ~kLod0;
            if (supportsMeshSuppression(replacement->path)) {
                std::lock_guard<std::mutex> lock(pairedPartMutex);
                pairedPartAssets.insert(replacement->path);
            }
            if (sceneMessages.fetch_add(1, std::memory_order_relaxed) < 240)
                logMask("Cleared paired stock LOD0; mask is", replacement->path.c_str(), mask);
        }
    }
    return result;
}

bool hasPairedReplacement(const char* path) {
    if (!supportsMeshSuppression(path))
        return false;
    std::lock_guard<std::mutex> lock(pairedPartMutex);
    return pairedPartAssets.count(path) != 0;
}

void suppressPairedLod0(unsigned char* model, const char* path) {
    auto& mask = *reinterpret_cast<std::uint16_t*>(model + 6);
    if (!(mask & kLod0) || !hasPairedReplacement(path))
        return;
    auto* mesh = *reinterpret_cast<unsigned char**>(model + 0x98);
    const auto count = *reinterpret_cast<const std::size_t*>(model + 0xA0);
    if (!mesh || !count || count > 4096)
        return;

    for (std::size_t index = 0; index < count; ++index, mesh += 0x1A0) {
        auto& meshMask = *reinterpret_cast<std::uint16_t*>(mesh + 0x124);
        meshMask &= ~kLod0;
    }
    mask &= ~kLod0;
    logMask("Suppressed LOD0 meshes in paired model; mask is", path, mask);
}

std::uintptr_t readSceneModel(void* record, void* reader, void* context) {
    const auto result = originalReadSceneModel(record, reader, context);
    auto* bytes = static_cast<unsigned char*>(record);
    char path[512];
    if (!isReading(reader) || !copyAssetPath(bytes + 0x18, path, sizeof path) || !isCarModel(path))
        return result;

    // The native carbin reader stores the rendering LOD mask here before
    // the scene builds the list of models to load.
    auto& mask = *reinterpret_cast<std::uint16_t*>(bytes + 0x1D2);
    if (sceneParts && (mask == kShowroom || (mask & kLod0)))
        sceneParts->push_back({bytes, path, mask});
    if (mask == kShowroom) {
        mask |= kLod0;
        if (sceneMessages.fetch_add(1, std::memory_order_relaxed) < 240)
            logMask("Promoted car-scene LOD mask to", path, mask);
    }
    return result;
}

void promoteParsedModel(unsigned char* model, const char* path) {
    auto& mask = *reinterpret_cast<std::uint16_t*>(model + 6);
    if (!(mask & kShowroom))
        return;
    auto* mesh = *reinterpret_cast<unsigned char**>(model + 0x98);
    const auto count = *reinterpret_cast<const std::size_t*>(model + 0xA0);
    if (!mesh || !count || count > 4096)
        return;

    std::vector<std::string> lod0Names;
    if (mask & kLod0) {
        for (std::size_t index = 0; index < count; ++index) {
            const auto* candidate = mesh + index * 0x1A0;
            if (!(*reinterpret_cast<const std::uint16_t*>(candidate + 0x124) & kLod0))
                continue;
            char name[256];
            if (copyAssetPath(candidate + 0x100, name, sizeof name)) {
                std::string_view key(name);
                if (endsWith(key, "_lods0"))
                    key.remove_suffix(6);
                else if (endsWith(key, "_lod0"))
                    key.remove_suffix(5);
                lod0Names.emplace_back(key);
            }
        }
    }

    unsigned promoted = 0;
    for (std::size_t index = 0; index < count; ++index, mesh += 0x1A0) {
        const auto material = *reinterpret_cast<const std::uint16_t*>(mesh + 0x120);
        auto& meshMask = *reinterpret_cast<std::uint16_t*>(mesh + 0x124);
        if (material == 0xFFFF || meshMask != kShowroom || mesh[0x126] != 0xFF || mesh[0x127] != 0xFF)
            continue;
        if (mask & kLod0) {
            char name[256];
            if (!copyAssetPath(mesh + 0x100, name, sizeof name))
                continue;
            std::string_view key(name);
            if (endsWith(key, "_lods"))
                key.remove_suffix(5);
            bool duplicatesLod0 = false;
            for (const auto& existing : lod0Names)
                duplicatesLod0 |= key == existing;
            if (duplicatesLod0)
                continue;
        }
        meshMask |= kLod0;
        mesh[0x126] = 0;
        ++promoted;
    }
    if (promoted) {
        if (model[1] == 0xFF)
            model[1] = 0;
        mask |= kLod0;
        if (modelMessages.fetch_add(1, std::memory_order_relaxed) < 240)
            logMask("Promoted parsed model LOD mask to", path, mask);
    }
}

bool observeModel(void* model, void* bundle, const void* member) {
    const bool loaded = originalParseModel(model, bundle, member);
    if (!loaded)
        return false;

    char path[512];
    // All three parser call sites pass resource+0xF0; the path is at +0xB8.
    if (!member || !copyAssetPath(reinterpret_cast<const unsigned char*>(
            reinterpret_cast<std::uintptr_t>(member) - 0x38), path, sizeof path))
        return true;
    if (isCarModel(path)) {
        auto* bytes = static_cast<unsigned char*>(model);
        promoteParsedModel(bytes, path);
        suppressPairedLod0(bytes, path);
    }
    const auto mask = *reinterpret_cast<const std::uint16_t*>(
        static_cast<const unsigned char*>(model) + 6);
    const bool car = std::strstr(path, "\\cars\\") != nullptr;
    const bool audiEngine = std::strstr(path, "aud_rs4avant_13\\scene\\exterior\\engine\\") != nullptr;
    const bool explicitShowroom = std::strstr(path, "__slod.modelbin") != nullptr;
    if (!car || (!audiEngine && mask != kShowroom && !explicitShowroom))
        return true;
    if (!audiEngine && modelMessages.fetch_add(1, std::memory_order_relaxed) >= 96)
        return true;

    logMask("Car model loaded with LOD mask", path, mask);
    return true;
}

void observeRenderMode(void* renderer) {
    // Both the stateflow setter and direct callers reach this function.
    const auto* bytes = static_cast<const unsigned char*>(renderer);
    const int mode = *reinterpret_cast<const int*>(bytes + 0x3CC);
    const int variant = *reinterpret_cast<const int*>(bytes + 0x3D0);
    char message[128];
    std::snprintf(message, sizeof message, "Applying dynamic render mode: %d (variant %d)",
                  mode, variant);
    platform->log(mod, message);
    originalApplyRenderMode(renderer);
}

std::uintptr_t observeControl(void* control, void* reader) {
    const auto result = originalReadControl(control, reader);
    if (*static_cast<const void* const*>(control) == autovistaControlVtable &&
        controlMessages.fetch_add(1, std::memory_order_relaxed) < 32) {
        const auto value = static_cast<const unsigned char*>(control)[8];
        char message[80];
        std::snprintf(message, sizeof message, "Parsed CarDrawAutovista=%u",
                      static_cast<unsigned>(value));
        platform->log(mod, message);
    }
    return result;
}

DWORD WINAPI install(void*) {
    auto* game = static_cast<unsigned char*>(platform->wait_for_code(60000));
    if (!game || std::memcmp(game + kParseModelRva, kParseModelCode, sizeof kParseModelCode)
              || std::memcmp(game + kApplyRenderModeRva, kApplyRenderModeCode, sizeof kApplyRenderModeCode)
              || std::memcmp(game + kReadAutovistaRva, kReadAutovistaCode, sizeof kReadAutovistaCode)
              || std::memcmp(game + kReadSceneModelRva, kReadSceneModelCode, sizeof kReadSceneModelCode)
              || std::memcmp(game + kReadSceneRva, kReadSceneCode, sizeof kReadSceneCode)) {
        platform->log(mod, "LOD probe unavailable on this game build.");
        return 0;
    }
    autovistaControlVtable = game + kAutovistaControlVtableRva;
    if (!platform->hook_function(mod, "Observe model LOD header", game + kParseModelRva,
                                 reinterpret_cast<void*>(observeModel), reinterpret_cast<void**>(&originalParseModel)) ||
        !platform->hook_function(mod, "Observe applied dynamic render mode", game + kApplyRenderModeRva,
                                 reinterpret_cast<void*>(observeRenderMode), reinterpret_cast<void**>(&originalApplyRenderMode)) ||
        !platform->hook_function(mod, "Observe CarDrawAutovista control parser", game + kReadAutovistaRva,
                                 reinterpret_cast<void*>(observeControl), reinterpret_cast<void**>(&originalReadControl)) ||
        !platform->hook_function(mod, "Promote car-scene LOD masks", game + kReadSceneModelRva,
                                 reinterpret_cast<void*>(readSceneModel), reinterpret_cast<void**>(&originalReadSceneModel)) ||
        !platform->hook_function(mod, "Pair showroom models with stock car-scene models", game + kReadSceneRva,
                                 reinterpret_cast<void*>(readScene), reinterpret_cast<void**>(&originalReadScene)) ||
        !platform->enable_hooks(mod, nullptr)) {
        platform->disable_hooks(mod, nullptr);
        platform->log(mod, "Could not enable LOD probe.");
        return 0;
    }
    platform->log(mod, "Runtime showroom LOD promotion active.");
    return 0;
}
}

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api || api->api_version < 3)
        return 1;
    FH2ModInfo info{};
    info.size = sizeof info;
    info.name = "FH2_AutovistaLayer";
    info.version = "0.3.2-RC2";
    info.github_repo = "suderek22/FH2_AutovistaLayer";
    info.check_updates = 1;
    info.description = "Includes showroom geometry when the game renders a car at LOD0.";
    mod = api->register_mod(&info);
    if (!mod)
        return 1;
    platform = api;
    HANDLE worker = CreateThread(nullptr, 0, install, nullptr, 0, nullptr);
    if (!worker)
        return 1;
    CloseHandle(worker);
    return 0;
}
