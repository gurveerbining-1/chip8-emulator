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

typedef struct{
    uint16_t opcode;
    uint16_t NNN;   // 12 bit address/constant
    uint8_t NN;     // 8 bit constant
    uint8_t N;      // 4 bit constant
    uint8_t X;      // 4 bit register identifier
    uint8_t Y;      // 4 bit register identifier
} instruction_t;

// CHIP-8 machine object
typedef struct{
    emulator_state_t state;
    uint8_t ram[4096]; 	 // 4096-byte memory space
    bool display[64*32]; // Emulate original chip-8 resolution pixels
    uint16_t stack[12];  // Subroutine stack
    uint16_t *stack_ptr;  // Stack pointer
    uint8_t V[16]; 		 // Data registers V0-VF
    uint16_t I; 		 // Index register
    uint16_t PC;         // Program Counter
    uint8_t delay_timer; // Decrements at 60hz when > 0
    uint8_t sound_timer; // Decrements at 60hz and plays tone when > 0
    bool keypad[16];	 // Hexadecimal keypad 0x0 - 0xF
    const char *rom_name;// Currently running ROM
    instruction_t inst;  // Currently executing instruction 
} chip8_t;

// Initialize SDL
bool init_sdl(sdl_t *sdl, const config_t config){
    if (SDL_Init((SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0 )){
        SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }
    
    sdl->window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);
    
    if (!sdl->window){
        SDL_Log("Could not create SDL window %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl->renderer){
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
        .bg_color = 0x00000000, // Black
        .scale_factor = 20,     // Default resolution will be 1280 x 640
    };

    // Override defaults from passed in arugments
    for(int i = 1; i < argc; i++){
        (void)argv[i]; // Prevent compiler error from unused variables argc/argv 
    }

    return true;
}

// Initialize chip8 machine
bool init_chip8(chip8_t *chip8, const char* rom_name){
    
    const uint32_t entry_point = 0x200; // Chip8 roms will be loaded to 0x200
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };
    // Load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // Open ROM file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom){
        SDL_Log("Could not find ROM! %s\n", SDL_GetError());
        return false;
    }

    // Get and check ROM size
    fseek(rom, 0, SEEK_END);
    const long rom_size = ftell(rom);
    const long max_size = sizeof chip8->ram - entry_point;
    rewind(rom); // rewind to make sure that when we read we are not at the end of the file

    if (rom_size > max_size){
        SDL_Log("ROM %s is too big! ROM size: %zu, MAX size: %zu\n", rom_name, rom_size, max_size);
        return false;
    }
    
    // Load ROM
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1){
        SDL_Log("Could not read ROM file %s into CHIP8 memory!\n", rom_name);
        return false;
    }

    fclose(rom); 

    // Set chip8 machine defaults
    chip8->state = RUNNING; // default state
    chip8->PC = entry_point; // start program counter at ROM entry point
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];
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
                    
                        case SDLK_SPACE:
                        // Space key, set the state as PAUSED or RUNNING
                        if(chip8->state == RUNNING){
                            chip8->state = PAUSED; // Paused
                            puts("=== PAUSED ====");
                        }
                        else{
                            chip8->state = RUNNING; // Resumed
                        }
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

#ifdef DEBUG
    void print_debug_info(chip8_t *chip8){
        printf("Address: 0x%04x, Opcode: 0x%04x, Desc: ", chip8->PC - 2, chip8->inst.opcode);
        switch ((chip8->inst.opcode >> 12) & 0x0F){
            case 0x0:
                if (chip8->inst.NN == 0xE0){
                    // 0x00E0: Clear the screen
                    printf("Clear screen\n");
                }
                else if (chip8->inst.NN == 0xEE){
                    // 0x00EE: Returns from a subroutine
                    // Set program counter to last address from subroutine stack (pop it off the stack) 
                    // so that the next opcode will be gotten from that address
                    printf("Return from subroutine to address 0x%04x\n", *(chip8->stack_ptr - 1));

                }
                else{
                    printf("Unimplemented Opcode\n");
                }
                break;

            case 0x02:
                // 0x02NNN: Call subroutine at NNN
                // Store current address to return to on subroutine stack (push it on the stack)
                // and set program counter to subroutine address so that the next opcode is gotten from there
                *chip8->stack_ptr++ = chip8->PC;
                chip8->PC = chip8->inst.NNN;
                break;
            
            case 0x0A:
                // 0xANNN: Set index register to NNN
                printf("Set I to NNN(0x%04X)\n", chip8->inst.NNN);
                break;
            default:
                printf("Unimplemented Opcode\n");
                break; // Unimplemented or invalid opcode
        }
    }
#endif

// Emulate 1 chip8 instruction
void emulate_instruction(chip8_t *chip8){
    // Get next opcode from ram
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];
    chip8->PC += 2; // incremented by 2 because each opcode is 2 bytes in length and the program counter holds the memory address of the next instruction to be executed

    // Fill out the current instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; 
    chip8->inst.NN = chip8->inst.opcode & 0x0FF; 
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F; 

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // Emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F){
        case 0x0:
            if (chip8->inst.NN == 0xE0){
                // 0x00E0: Clear the screen
                memset(&chip8->display[0], false, sizeof chip8->display);
            }
            else if (chip8->inst.NN == 0xEE){
                // 0x00EE: Returns from a subroutine
                // Set program counter to last address from subroutine stack (pop it off the stack) 
                // so that the next opcode will be gotten from that address
                chip8->PC = *--chip8->stack_ptr; // Decrement then grab the stack location and then store in program counter
            }
            break;

        case 0x02:
            // 0x02NNN: Call subroutine at NNN
            // Store current address to return to on subroutine stack (push it on the stack)
            // and set program counter to subroutine address so that the next opcode is gotten from there
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;
        
        case 0x0A:
            // 0xANNN: Set index register to NNN
            chip8->I = chip8->inst.NNN;
            break;

        default:
            break; // Unimplemented or invalid opcode
    }

}

int main(int argc, char **argv){
    // Default usage message for args
    if(argc < 2){
        fprintf(stderr, "Usage %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE); 
    }  

    // Initalize emulator configuration/options
    config_t config = {0};
    if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);
    
    // Initalize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    // Initalize CHIP-8 machine
    chip8_t chip8 = {0};
    const char *rom_name = argv[1]; // rom name is entered on command line 
    if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    // Initial screen clear
    clear_screen(sdl, config);

    
    // Main emulator loop
    while(chip8.state != QUIT){
        // Handle user input
        handle_input(&chip8);
        if (chip8.state == PAUSED) continue;

        // Get_Time();
        // Emulate CHIP-8 Instructions
        emulate_instruction(&chip8);
        // Get_Time() elapsed since last Get_Time();

        // Delay for approximately 60hz/60fps (16.67 ms)
        SDL_Delay(16);

        // Update window with changes
        update_screen(sdl);
    }

    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}
