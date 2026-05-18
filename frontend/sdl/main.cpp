#include "gameboy.hpp"

#include <SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

namespace {

constexpr int kScale = 4;
constexpr int kWidth = static_cast<int>(gb::kLcdWidth);
constexpr int kHeight = static_cast<int>(gb::kLcdHeight);
constexpr std::array<uint32_t, 4> kPalette = {0xFFE0F8D0u, 0xFF88C070u, 0xFF346856u, 0xFF081820u};

} // namespace

int main(int argc, char* argv[]) {
    static_cast<void>(argc);
    static_cast<void>(argv);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "sdl init failed: %s\n", SDL_GetError());
        return 1;
    }

    const int pos = SDL_WINDOWPOS_CENTERED;
    SDL_Window* window = SDL_CreateWindow("gbemu", pos, pos, kWidth * kScale, kHeight * kScale, 0);
    if (window == nullptr) {
        std::fprintf(stderr, "sdl window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const uint32_t renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, renderer_flags);
    if (renderer == nullptr) {
        std::fprintf(stderr, "sdl renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const uint32_t texture_format = SDL_PIXELFORMAT_ARGB8888;
    const int texture_access = SDL_TEXTUREACCESS_STREAMING;
    SDL_Texture* texture = SDL_CreateTexture(renderer, texture_format, texture_access, kWidth, kHeight);
    if (texture == nullptr) {
        std::fprintf(stderr, "sdl texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    gb::Gameboy gameboy;
    std::array<uint32_t, gb::kLcdWidth * gb::kLcdHeight> pixels{};

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        gameboy.run_frame();
        const std::span<const uint8_t> fb = gameboy.framebuffer();
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = kPalette[fb[i] & 0x3u];
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(), kWidth * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
