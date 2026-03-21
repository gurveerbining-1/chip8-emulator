#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"

// SDL container object
typedef struct{
    SDL_Window* window;
    SDL_Renderer* renderer;
} sdl_t;

// Emulator configuration object
typedef struct{
    uint32_t window_width;  // SDL window width
    uint32_t window_height; // SDL window height
    uint32_t fg_color;      // Foreground colour RGBA8888
    uint32_t bg_color;      // Background colour RGBA8888
    uint32_t scale_factor;  // Amount to scale a chip-8 pixel by, e.g. 20x will be a 20x larger window
} config_t;

// Emulator states
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

// CHIP-8 machine object
typedef struct{
    emulator_state_t state;
} chip8_t;

// Initialize SDL
bool init_sdl(sdl_t *sdl, const config_t config){
    if (SDL_Init((SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0 )){
        SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }
    
    sdl->window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);
    
    if(!sdl->window){
        SDL_Log("Could not create SDL window %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("Could not create SDL renderer %s\n", SDL_GetError());
        return false;
    }


    return true;
}

// Set up inital emulator configuration from passed in arugments
bool set_config_from_args(config_t *config, const int argc, char **argv){

    // Set defaults
    *config = (config_t){
        .window_width = 64,     // CHIP-8 original x resolution
        .window_height = 32,    // CHIP-8 original y resolution
        .fg_color = 0xFFFFFFFF, // White
        .bg_color = 0xFFFF00FF, // Yellow
        .scale_factor = 20,     // Default resolution will be 1280 x 640
    };

    // Override defaults from passed in arugments
    for(int i = 1; i < argc; i++){
        (void)argv[i]; // Prevent compiler error from unused variables argc/argv 
    }

    return true;
}

// Initialize chip8 machine
bool init_chip8(chip8_t *chip8){
    chip8->state = RUNNING; // default state
    return true; // success
}

// Final cleanup
void final_cleanup(const sdl_t sdl){
    SDL_DestroyWindow(sdl.window); //destroy window
    SDL_DestroyRenderer(sdl.renderer); //destroy renderer
    SDL_Quit(); // shut down SDL subsytems
}

// Clear screen / SDL window to background colour
void clear_screen(const sdl_t sdl, const config_t config){
    // extract r g b a values from bg.color
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);

}

// Update window with any changes
void update_screen(const sdl_t sdl){
    SDL_RenderPresent(sdl.renderer);
}

// Handle user input
void handle_input(chip8_t *chip8){
    SDL_Event event;

    // While there is still an event in the queue
    while (SDL_PollEvent(&event)){
        switch (event.type){
            case SDL_QUIT:
                // Exit window; end program
                chip8->state = QUIT; // will exit main emulator loop
                return;
            
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        // Escape key, exit window and end program
                        chip8->state = QUIT; // will exit main emulator loop
                        return;
                    
                    default:
                        break;
                }
                break;

            case SDL_KEYUP:
                break;
            
            default:
                break;
        }
    }
}

int main(int argc, char **argv){
  
    // Initalize emulator configuration/options
    config_t config = {0};
    if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);
    
    // Initalize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    // Initalize CHIP-8 machine
    chip8_t chip8 = {0};
    if(!init_chip8(&chip8)) exit(EXIT_FAILURE);

    // Initial screen clear
    clear_screen(sdl, config);

    
    // Main emulator loop
    while(chip8.state != QUIT){
        // Handle user input
        handle_input(&chip8);
        // if (chip8.state == PAUSED) continue;

        // Get_Time();
        // Emulate CHIP-8 Instructions
        // Get_Time() elapsed since last Get_Time();

        // Delay for approximately 60hz/60fps (16.67 ms)
        SDL_Delay(16);

        // Update window with changes
        update_screen(sdl);
    }

    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}