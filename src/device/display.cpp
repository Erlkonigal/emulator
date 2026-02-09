#include "emulator/device/display.h"

#include <cstring>

#include <SDL2/SDL.h>
#include <mutex>

struct SdlDisplayDevice::SdlDisplayState {
    SDL_Window* Window = nullptr;
    SDL_Renderer* Renderer = nullptr;
    SDL_Texture* Texture = nullptr;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint8_t* FrameBuffer = nullptr;
    bool Ready = false;
    bool Headless = false;
};

namespace {
constexpr uint64_t kRegCtrl = 0x00;
constexpr uint64_t kRegWidth = 0x04;
constexpr uint64_t kRegHeight = 0x08;
constexpr uint64_t kRegPitch = 0x0c;
constexpr uint64_t kRegStatus = 0x10;
constexpr uint64_t kRegKeyData = 0x20;
constexpr uint64_t kRegKeyStatus = 0x24;
constexpr uint64_t kRegKeyLast = 0x28;

constexpr uint64_t kStatusReady = 1ull << 0;
constexpr uint64_t kStatusDirty = 1ull << 1;

constexpr uint32_t kKeyStatusReady = 1u << 0;
}

SdlDisplayDevice::SdlDisplayDevice()
    : State(new SdlDisplayState()) {
    SetType(DeviceType::Display);
    SetReadHandler([this](const MemAccess& access) { return HandleRead(access); });
    SetWriteHandler([this](const MemAccess& access) { return HandleWrite(access); });
}

SdlDisplayDevice::~SdlDisplayDevice() {
    Shutdown();
    delete State;
}

bool SdlDisplayDevice::Init(uint32_t width, uint32_t height, const char* title) {
    if (State == nullptr || width == 0 || height == 0) {
        return false;
    }
    Shutdown();
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return false;
        }
    }
    State->Width = width;
    State->Height = height;
    State->Window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(width), static_cast<int>(height), SDL_WINDOW_SHOWN);
    if (State->Window == nullptr) {
        return false;
    }
    State->Renderer = SDL_CreateRenderer(State->Window, -1, SDL_RENDERER_ACCELERATED);
    if (State->Renderer == nullptr) {
        return false;
    }
    State->Texture = SDL_CreateTexture(State->Renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, static_cast<int>(width), static_cast<int>(height));
    if (State->Texture == nullptr) {
        return false;
    }
    State->FrameBuffer = new uint8_t[static_cast<size_t>(width) * height * 4];
    std::memset(State->FrameBuffer, 0, static_cast<size_t>(width) * height * 4);
    State->Ready = true;
    State->Headless = false;
    Dirty.store(true, std::memory_order_release);
    return true;
}

bool SdlDisplayDevice::InitHeadless(uint32_t width, uint32_t height) {
    if (State == nullptr || width == 0 || height == 0) {
        return false;
    }
    Shutdown();
    State->Width = width;
    State->Height = height;
    State->FrameBuffer = new uint8_t[static_cast<size_t>(width) * height * 4];
    std::memset(State->FrameBuffer, 0, static_cast<size_t>(width) * height * 4);
    State->Ready = true;
    State->Headless = true;
    Dirty.store(true, std::memory_order_release);
    return true;
}

void SdlDisplayDevice::Shutdown() {
    if (State == nullptr) {
        return;
    }
    State->Headless = false;
    delete[] State->FrameBuffer;
    State->FrameBuffer = nullptr;
    if (State->Texture != nullptr) {
        SDL_DestroyTexture(State->Texture);
        State->Texture = nullptr;
    }
    if (State->Renderer != nullptr) {
        SDL_DestroyRenderer(State->Renderer);
        State->Renderer = nullptr;
    }
    if (State->Window != nullptr) {
        SDL_DestroyWindow(State->Window);
        State->Window = nullptr;
    }
    State->Width = 0;
    State->Height = 0;
    State->Ready = false;
    Dirty.store(false, std::memory_order_release);
    PresentRequested.store(false, std::memory_order_release);
}

bool SdlDisplayDevice::IsReady() const {
    return State != nullptr && State->Ready;
}

void SdlDisplayDevice::PollEvents(uint32_t timeoutMs) {
    if (State == nullptr || !State->Ready) {
        return;
    }
    if (State->Headless) {
        return;
    }
    SDL_Event event;
    if (timeoutMs > 0) {
        if (SDL_WaitEventTimeout(&event, static_cast<int>(timeoutMs)) != 0) {
            std::lock_guard<std::mutex> lock(InputMutex);
            if (event.type == SDL_QUIT) {
                QuitRequested = true;
            }
            if (event.type == SDL_KEYDOWN) {
                LastKey = static_cast<uint32_t>(event.key.keysym.sym);
                KeyQueue.push_back(LastKey);
            }
        }
    }
    while (SDL_PollEvent(&event)) {
        std::lock_guard<std::mutex> lock(InputMutex);
        if (event.type == SDL_QUIT) {
            QuitRequested = true;
        }
        if (event.type == SDL_KEYDOWN) {
            LastKey = static_cast<uint32_t>(event.key.keysym.sym);
            KeyQueue.push_back(LastKey);
        }
    }
}

bool SdlDisplayDevice::IsQuitRequested() const {
    std::lock_guard<std::mutex> lock(InputMutex);
    return QuitRequested;
}

void SdlDisplayDevice::PushKey(uint32_t key) {
    std::lock_guard<std::mutex> lock(InputMutex);
    LastKey = key;
    KeyQueue.push_back(key);
}

uint32_t SdlDisplayDevice::GetWidth() const {
    return State ? State->Width : 0;
}

uint32_t SdlDisplayDevice::GetHeight() const {
    return State ? State->Height : 0;
}

uint32_t SdlDisplayDevice::GetPitch() const {
    return State ? State->Width * 4u : 0u;
}

uint64_t SdlDisplayDevice::GetFrameBufferSize() const {
    if (State == nullptr) {
        return 0;
    }
    return static_cast<uint64_t>(State->Width) * State->Height * 4u;
}

uint64_t SdlDisplayDevice::GetMappedSize() const {
    return kFrameBufferOffset + GetFrameBufferSize();
}

bool SdlDisplayDevice::IsDirty() const {
    return Dirty.load(std::memory_order_acquire);
}

bool SdlDisplayDevice::IsPresentRequested() const {
    return PresentRequested.load(std::memory_order_acquire);
}

bool SdlDisplayDevice::ConsumePresentRequest() {
    return PresentRequested.exchange(false, std::memory_order_acq_rel);
}

void SdlDisplayDevice::Present() {
    if (State == nullptr || !State->Ready) {
        return;
    }
    if (State->Headless) {
        Dirty.store(false, std::memory_order_release);
        return;
    }
    std::lock_guard<std::mutex> lock(FrameMutex);
    SDL_UpdateTexture(State->Texture, nullptr, State->FrameBuffer,
        static_cast<int>(State->Width) * 4);
    SDL_RenderClear(State->Renderer);
    SDL_RenderCopy(State->Renderer, State->Texture, nullptr, nullptr);
    SDL_RenderPresent(State->Renderer);
    Dirty.store(false, std::memory_order_release);
}

uint32_t SdlDisplayDevice::GetUpdateFrequency() const {
    return 60;
}

bool SdlDisplayDevice::ReadRegister(uint64_t offset, uint64_t* value) {
    if (value == nullptr) {
        return false;
    }
    if (offset == kRegKeyData) {
        std::lock_guard<std::mutex> lock(InputMutex);
        if (KeyQueue.empty()) {
            *value = 0;
            return true;
        }
        *value = KeyQueue.front();
        KeyQueue.pop_front();
        return true;
    }
    if (offset == kRegKeyStatus) {
        std::lock_guard<std::mutex> lock(InputMutex);
        *value = KeyQueue.empty() ? 0u : kKeyStatusReady;
        return true;
    }
    if (offset == kRegKeyLast) {
        std::lock_guard<std::mutex> lock(InputMutex);
        *value = LastKey;
        return true;
    }
    if (offset == kRegWidth) {
        *value = GetWidth();
        return true;
    }
    if (offset == kRegHeight) {
        *value = GetHeight();
        return true;
    }
    if (offset == kRegPitch) {
        *value = GetPitch();
        return true;
    }
    if (offset == kRegStatus) {
        uint64_t status = 0;
        if (IsReady()) {
            status |= kStatusReady;
        }
        if (Dirty.load(std::memory_order_acquire)) {
            status |= kStatusDirty;
        }
        *value = status;
        return true;
    }
    return false;
}

bool SdlDisplayDevice::WriteRegister(uint64_t offset, uint64_t value) {
    if (offset == kRegCtrl) {
        if ((value & 1u) != 0) {
            PresentRequested.store(true, std::memory_order_release);
        }
        return true;
    }
    if (offset == kRegKeyStatus) {
        std::lock_guard<std::mutex> lock(InputMutex);
        (void)value;
        KeyQueue.clear();
        LastKey = 0;
        return true;
    }
    return false;
}

MemResponse SdlDisplayDevice::HandleRead(const MemAccess& access) {
    MemResponse response;
    if (State == nullptr || access.Size == 0 || access.Size > sizeof(uint64_t)) {
        response.Success = false;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    uint64_t mappedSize = GetMappedSize();
    if (access.Address >= mappedSize || access.Address > mappedSize - access.Size) {
        response.Success = false;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    if (access.Address < kFrameBufferOffset) {
        uint64_t value = 0;
        if (!ReadRegister(access.Address, &value)) {
            response.Success = false;
            response.Error.Type = CpuErrorType::AccessFault;
            response.Error.Address = access.Address;
            response.Error.Size = access.Size;
            return response;
        }
        response.Success = true;
        response.Data = value;
        return response;
    }
    uint64_t fbOffset = access.Address - kFrameBufferOffset;
    std::lock_guard<std::mutex> lock(FrameMutex);
    uint64_t value = 0;
    for (uint32_t i = 0; i < access.Size; ++i) {
        value |= static_cast<uint64_t>(State->FrameBuffer[static_cast<size_t>(fbOffset + i)])
            << (8 * i);
    }
    response.Success = true;
    response.Data = value;
    return response;
}

MemResponse SdlDisplayDevice::HandleWrite(const MemAccess& access) {
    MemResponse response;
    if (State == nullptr || access.Size == 0 || access.Size > sizeof(uint64_t)) {
        response.Success = false;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    uint64_t mappedSize = GetMappedSize();
    if (access.Address >= mappedSize || access.Address > mappedSize - access.Size) {
        response.Success = false;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    if (access.Address < kFrameBufferOffset) {
        if (!WriteRegister(access.Address, access.Data)) {
            response.Success = false;
            response.Error.Type = CpuErrorType::AccessFault;
            response.Error.Address = access.Address;
            response.Error.Size = access.Size;
            return response;
        }
        response.Success = true;
        return response;
    }
    uint64_t fbOffset = access.Address - kFrameBufferOffset;
    std::lock_guard<std::mutex> lock(FrameMutex);
    uint64_t value = access.Data;
    for (uint32_t i = 0; i < access.Size; ++i) {
        State->FrameBuffer[static_cast<size_t>(fbOffset + i)] =
            static_cast<uint8_t>(value & 0xff);
        value >>= 8;
    }
    Dirty.store(true, std::memory_order_release);
    response.Success = true;
    return response;
}
