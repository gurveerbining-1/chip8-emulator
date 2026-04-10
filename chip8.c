#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"
#include <time.h>

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
    bool pixel_outline;     // Option for user to have a more grid like UI
    uint32_t clock_rate;    // instructions per second (hz)
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
        .pixel_outline = true,  // Draw pixel outlines set to on by default
        .clock_rate = 700,      // process 700 instructions per second
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
void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8){
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
    
    // Grab colour values to draw
    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >> 8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >> 0) & 0xFF;
    
    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >> 0) & 0xFF;


    // Loop through display pixels, draw rectangle per pixel
    for (uint32_t i = 0; i < sizeof chip8.display; i++){
        // Translate 1D index i value to 2D x,y coordinates
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i]){
            // if pixel is on draw foreground colour
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            if (config.pixel_outline) {
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);
            }
        }
        else{
            // if pixel is off draw background colour
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }
    SDL_RenderPresent(sdl.renderer);
}

// Handle user input
// CHIP8 keypad,    QWERTY
// 123C             1234
// 456D             qwer
// 789E             asdf
// A0BF             zxcv
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
                        
                        // map keys to chip8 keypad
                        case SDLK_1: chip8->keypad[0x1] = true; break;
                        case SDLK_2: chip8->keypad[0x2] = true; break;
                        case SDLK_3: chip8->keypad[0x3] = true; break;
                        case SDLK_4: chip8->keypad[0xC] = true; break;
                        
                        case SDLK_q: chip8->keypad[0x4] = true; break;
                        case SDLK_w: chip8->keypad[0x5] = true; break;
                        case SDLK_e: chip8->keypad[0x6] = true; break;
                        case SDLK_r: chip8->keypad[0xD] = true; break;
                        
                        case SDLK_a: chip8->keypad[0x7] = true; break;
                        case SDLK_s: chip8->keypad[0x8] = true; break;
                        case SDLK_d: chip8->keypad[0x9] = true; break;
                        case SDLK_f: chip8->keypad[0xE] = true; break;
                        
                        case SDLK_z: chip8->keypad[0xA] = true; break;
                        case SDLK_x: chip8->keypad[0x0] = true; break;
                        case SDLK_c: chip8->keypad[0xB] = true; break;
                        case SDLK_v: chip8->keypad[0xF] = true; break;

                    default:
                        break;
                }
                break;

            case SDL_KEYUP:
                switch(event.key.keysym.sym){

                case SDLK_1: chip8->keypad[0x1] = false; break;
                case SDLK_2: chip8->keypad[0x2] = false; break;
                case SDLK_3: chip8->keypad[0x3] = false; break;
                case SDLK_4: chip8->keypad[0xC] = false; break;
                        
                case SDLK_q: chip8->keypad[0x4] = false; break;
                case SDLK_w: chip8->keypad[0x5] = false; break;
                case SDLK_e: chip8->keypad[0x6] = false; break;
                case SDLK_r: chip8->keypad[0xD] = false; break;
                        
                case SDLK_a: chip8->keypad[0x7] = false; break;
                case SDLK_s: chip8->keypad[0x8] = false; break;
                case SDLK_d: chip8->keypad[0x9] = false; break;
                case SDLK_f: chip8->keypad[0xE] = false; break;
                        
                case SDLK_z: chip8->keypad[0xA] = false; break;
                case SDLK_x: chip8->keypad[0x0] = false; break;
                case SDLK_c: chip8->keypad[0xB] = false; break;
                case SDLK_v: chip8->keypad[0xF] = false; break;
            
                default:
                    break;
            }
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

            case 0x01:
                // 0x01NNN: Jump to address NNN
                // This instruction should simply set PC to NNN, 
                // causing the program to jump to that memory location. 
                // Do not increment the PC afterwards, it jumps directly there.
                printf("Jump to address 0x%04X\n",chip8->inst.NNN);
                break;    

            case 0x02:
                // 0x02NNN: Call subroutine at NNN
                // Store current address to return to on subroutine stack (push it on the stack)
                // and set program counter to subroutine address so that the next opcode is gotten from there
                *chip8->stack_ptr++ = chip8->PC;
                chip8->PC = chip8->inst.NNN;
                break;
            
            case 0x03:
                // 0x03NNN: Skips the next instruction if VX equals NN (usually the next instruction is a jump to skip a code block)
                printf("Skip the next instruction if V%X (0x%02x) equals NN (0x%02x)\n", 
                    chip8->inst.X ,chip8->V[chip8->inst.X], chip8->inst.NN);
                break;  
            
            case 0x04:
                // 0x04NNN: Skips the next instruction if VX does not equal NN (usually the next instruction is a jump to skip a code block)
                printf("Skip the next instruction if V%X (0x%02x) does not equal NN (0x%02x)\n", 
                    chip8->inst.X ,chip8->V[chip8->inst.X], chip8->inst.NN);
                break;  
            
            case 0x05:
                // 0x05XY0: Skips the next instruction if VX equals VY (usually the next instruction is a jump to skip a code block)
                printf("Skip the next instruction if V%X (0x%02x) equals V%X (0x%02x)\n",
                chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
                break;

            case 0x06:
                // 0x6NNN: Sets VX to NN 
                printf("Set register V%X = NN (0x%02X)\n",
                   chip8->inst.X, chip8->inst.NN);
                break;

            case 0x07:
                // 0x7NNN: Adds NN to VX (carry flag is not changed).
                printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n",
                   chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN,
                   chip8->V[chip8->inst.X] + chip8->inst.NN);
                break;
           
            case 0x08:
                switch(chip8->inst.N){
                    case 0:
                        // 0x8XY0: Sets VX to the value of VY 
                        printf("Set register V%X = V%X (0x%02X)\n", chip8->inst.X, 
                            chip8->inst.Y, 
                            chip8->V[chip8->inst.Y]);
                        break;
                    
                    case 1:
                        // 0x8XY1: Sets VX to VX or VY. (bitwise OR operation)
                        printf("Set register V%X (0x%02X) |= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, 
                            chip8->V[chip8->inst.X],
                            chip8->inst.Y, 
                            chip8->V[chip8->inst.Y],
                            chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
                        break;
                    
                    case 2:
                        // 0x8XY2: Sets VX to VX and VY. (bitwise AND operation)
                        printf("Set register V%X (0x%02X) &= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, 
                            chip8->V[chip8->inst.X],                            
                            chip8->inst.Y, 
                            chip8->V[chip8->inst.Y],
                            chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);                        
                        break;                    

                    case 3:
                        // 0x8XY3: Sets VX to VX xor VY
                        printf("Set register V%X (0x%02X) ^= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, 
                            chip8->V[chip8->inst.X],
                            chip8->inst.Y,
                            chip8->V[chip8->inst.Y], 
                            chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
                        break;                     

                    case 4:
                        // 0x8XY4: Adds VY to VX. VF is set to 1 when there's an overflow, and to 0 when there is not
                        printf("Set V%X += V%X. VF is set to 1 if theres a carry flag. Result: 0x%02X, VF = %X\n", chip8->inst.X,
                        chip8->inst.Y,
                        chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                        ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255) // value of carry
                        );
                        break;
                    
                    case 5:
                        // 0x8XY5: VY is subtracted from VX. VF is set to 0 when there's an underflow, and 1 when there is not. (i.e. VF set to 1 if VX >= VY and 0 if not)
                        printf("Set V%X -= V%X. VF is set to 1 if theres a carry flag. Result: 0x%02X, VF = %X\n", chip8->inst.X,
                        chip8->inst.Y,
                        chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                        (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]) // value of carry
                        );
                        break;
                    
                    case 6:
                        // 0x8XY6: Shifts VX to the right by 1, then stores the least significant bit of VX prior to the shift into VF
                        printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                           chip8->inst.X, chip8->V[chip8->inst.X],
                           chip8->V[chip8->inst.X] & 1,
                           chip8->V[chip8->inst.X] >> 1);                        
                        break;
                    
                    case 7:
                        // 0X8XY7: Sets VX to VY minus VX. VF is set to 0 when there's an underflow, and 1 when there is not. (i.e. VF set to 1 if VY >= VX)
                        printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                           chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y],
                           chip8->inst.X, chip8->V[chip8->inst.X],
                           chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
                           (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
                        break;
                    
                    case 0xE:
                        // 0x8XYE: Shifts VX to the left by 1, then sets VF to 1 if the most significant bit of VX prior to that shift was set, or to 0 if it was unset
                        printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                           chip8->inst.X, chip8->V[chip8->inst.X],
                           (chip8->V[chip8->inst.X] & 0x80) >> 7,
                           chip8->V[chip8->inst.X] << 1);
                        break;

                    default:
                        break; // Unimplemented or invalid opcode
                    }
                    break;
                    
            case 0x0A:
                // 0xANNN: Set index register to NNN
                printf("Set I to NNN(0x%04X)\n", chip8->inst.NNN);
                break;
        
            case 0x0C:
                // 0xCNNN: Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
                printf("Set V%X = rand() %% 256 & NN (0x%02x)\n", 
                chip8->inst.X, chip8->inst.NN);
                break;
            
            case 0x0D:
                // OxDXYN: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels 
                printf("Draw N (%u) height sprite at coordinates V%X (0x%02x), V%X (0x%02x) from memory location I (0x%04X), set VF = 1 if any pixels are turned off\n", 
                    chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
                break;

            case 0x0E:
                switch(chip8->inst.NN){
                    case 0x9E:
                        // 0xEX9E: Skips the next instruction if the key stored in VX(only consider the lowest nibble) is pressed (usually the next instruction is a jump to skip a code block). 
                        printf("Skip next instruction if key in V%X (0x%02x) is pressed, keypad value: %d\n", chip8->inst.X,
                        chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);
                        break;
                    
                    case 0xA1:
                        // 0xEXA1: Skips the next instruction if the key stored in VX(only consider the lowest nibble) is not pressed (usually the next instruction is a jump to skip a code block).
                        printf("Skip next instruction if key in V%X (0x%02x) is NOT pressed, keypad value: %d\n", chip8->inst.X,
                        chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);

                        break;
                    
                }
                break;
            
            case 0x0F:
                switch(chip8->inst.NN){
                    case 0x0A:
                        // 0xFX0A: A key press is awaited, and then stored in VX (blocking operation, all instruction halted until next key event, delay and sound timers should continue processing).
                        printf("Await for key to be pressed and then stored in V%X\n", chip8->inst.X);
                        break;
                    
                    case 0x1E:
                        // 0xFX1E: Adds VX to I. VF is not affected.
                        printf("I (0x%04X) += V%X (0x%02X)\n", chip8->I, 
                            chip8->inst.X, chip8->V[chip8->inst.X]);
                        break;
                    
                    case 0x07:
                        // 0xF07: Sets VX to the value of the delay timer
                        printf("Set V%X = delay timer (0x%02X)\n", chip8->inst.X,
                        chip8->delay_timer);
                        break;

                    case 0x15:
                        // 0xF15: Sets delay timer to the value of VX
                        printf("Set delay timer (0x%02X) = V%X\n", chip8->delay_timer,
                        chip8->inst.X);                        
                        break;

                    case 0x18:
                        // 0xF18: Sets sound timer to the value of VX
                        printf("Set sound timer (0x%02X) = V%X\n", chip8->sound_timer,
                        chip8->inst.X);                                                
                        break;
                    
                    case 0x29:
                        // 0xF29: Sets I to the location of the sprite for the character in VX
                        printf("Set I to the sprite location in memory for the character in V%X (0x%2X). Result (VX * 5) = (0x%02X)\n", chip8->inst.X,
                        chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
                        break;
        
                    case 0x33:
                        // 0xF33: Stores the binary-coded decimal representation of VX, 
                        // with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
                        printf("Store BCD representation of V%X (0x%02X) at memory from I (0x%04X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                        break;
                    
                    case 0x55:
                        // 0xF55: Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
                        printf("Register dump V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                        break;
                   
                    case 0x65:
                        // 0xF65: Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified.
                        printf("Register load V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                        break;

                    break;
                }
                break;

            default:
                printf("Unimplemented Opcode\n");
                break; // Unimplemented or invalid opcode
        }
    }
#endif

// Emulate 1 chip8 instruction
void emulate_instruction(chip8_t *chip8, const config_t config){
    // Get next opcode from ram
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];
    chip8->PC += 2; // incremented by 2 because each opcode is 2 bytes in length and the program counter holds the memory address of the next instruction to be executed

    // Fill out the current instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; 
    chip8->inst.NN = chip8->inst.opcode & 0x0FF; 
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F; 

    bool carry;   // Save carry flag/VF value for some instructions

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

        case 0x01:
            // 0x01NNN: Jump to address NNN
            // This instruction should simply set PC to NNN, 
            // causing the program to jump to that memory location. 
            // Do not increment the PC afterwards, it jumps directly there.
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02:
            // 0x02NNN: Call subroutine at NNN
            // Store current address to return to on subroutine stack (push it on the stack)
            // and set program counter to subroutine address so that the next opcode is gotten from there
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;
        
        case 0x03:
            // 0x03NNN: Skips the next instruction if VX equals NN (usually the next instruction is a jump to skip a code block)
            if (chip8->V[chip8->inst.X] == chip8->inst.NN){
                chip8->PC += 2; // skip the next 2 byte opcode instruction
            }
            break;
        
        case 0x04:
            // 0x04NNN: Skips the next instruction if VX does not equal NN (usually the next instruction is a jump to skip a code block)
            if (chip8->V[chip8->inst.X] != chip8->inst.NN){
                chip8->PC += 2; // skip the next 2 byte opcode instruction
            }
            break;
        
        case 0x05:
            // 0x05XY0: Skips the next instruction if VX equals VY (usually the next instruction is a jump to skip a code block)
            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
                chip8->PC += 2; // skip the next 2 byte opcode instruction
            }
            break;

        case 0x06:
            // 0x6NNN: Sets VX to NN 
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            // 0x7NNN: Adds NN to VX (carry flag is not changed).
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;
        
        case 0x08:
            switch(chip8->inst.N){
                case 0:
                    // 0x8XY0: Sets VX to the value of VY 
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    break;
                
                case 1:
                    // 0x8XY1: Sets VX to VX or VY. (bitwise OR operation)
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y];
                    break;
                
                case 2:
                    // 0x8XY2: Sets VX to VX and VY. (bitwise AND operation)
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y];
                    break;                    

                case 3:
                    // 0x8XY3: Sets VX to VX xor VY
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y];
                    break;                     

                case 4:
                    // 0x8XY4: Adds VY to VX. VF is set to 1 when there's an overflow, and to 0 when there is not
                    carry = ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);
                   
                    // if adding the two registers results in a value greater than 255 set VF = 1
                  
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;
                
                case 5:
                    // 0x8XY5: VY is subtracted from VX. VF is set to 0 when there's an underflow, and 1 when there is not. (i.e. VF set to 1 if VX >= VY and 0 if not)
                    carry = (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]);
                    // if subtracting the two regiers results in a negative value then set VF = 0
                    
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];  
                    chip8->V[0xF] = carry;                   
                    break;
                
                case 6:
                    // 0x8XY6: Shifts VX to the right by 1, then stores the least significant bit of VX prior to the shift into VF
                    chip8->V[0xF] = (chip8->V[chip8->inst.X]) & 0x1;
                    chip8->V[chip8->inst.X] = (chip8->V[chip8->inst.X]) >> 1;
                    
                    break;
                
                case 7:
                    // 0X8XY7: Sets VX to VY minus VX. VF is set to 0 when there's an underflow, and 1 when there is not. (i.e. VF set to 1 if VY >= VX)
                    carry = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = carry;

                    break;
                
                case 0xE:
                    // 0x8XYE: Shifts VX to the left by 1, then sets VF to 1 if the most significant bit of VX prior to that shift was set, or to 0 if it was unset
                    uint8_t vx = chip8->V[chip8->inst.X];
                    chip8->V[0xF] = (vx >> 7) & 0x1; // store MSB before shift
                    chip8->V[chip8->inst.X] = vx << 1; // shift VX left by 1
                    
                    break;

                default:
                    break; // Unimplemented or invalid opcode

            }
            break;
        
        case 0x09:
            // 0x9xY0: Skips the next instruction if VX does not equal VY. (Usually the next instruction is a jump to skip a code block)
            
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){
                chip8->PC += 2;
            }
            break;

        case 0x0A:
            // 0xANNN: Set index register to NNN
            chip8->I = chip8->inst.NNN;
            break;
        
        case 0x0B:
            // 0xBNNN: Jumps to the address NNN plus V0
            chip8->PC = chip8->inst.NNN + chip8->V[0];
            break;

        case 0x0C:
            // 0xCNNN: Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;
        
        case 0x0D:
            // OxDXYN: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels 
            // and a height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; 
            // I value does not change after the execution of this instruction
            // VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that does not happen
            uint8_t x_coordinate = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t y_coordinate = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_x = x_coordinate; // Original X value, used to keep track of the initial point to draw from for the next row
            chip8->V[0xF] = 0; // set VF = 0

            // Loop over all N rows of the sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++){
                // Get the Nth byte of sprite data, counting from the memory address in the I register (I is not incremented)
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                x_coordinate = orig_x; // reset x for next row to draw

                // For each of the 8 pixels/bits in this sprite row (from left to right, ie. from most to least significant bit):
                for (int j = 7; j >= 0; j--){
                    bool *pixel = &chip8->display[y_coordinate * config.window_width + x_coordinate];
                    const bool sprite_bit = (sprite_data & 1 << j);

                    if (sprite_bit && *pixel){
                        chip8->V[0xF] = 1;
                    }

                    // XOR pixel with sprite pixel/bit to set it on or off
                    *pixel ^= sprite_bit;

                    // stop drawing if row if it hits the edge of the screen
                    if (++x_coordinate >= config.window_width){
                        break;
                    }
                }
                 // stop drawing sprite if it hits the bottom edge of the screen
                if (++y_coordinate >= config.window_height){
                    break;
                }

            }

            break;
        
        case 0x0E:
            switch(chip8->inst.NN){
                case 0x9E:
                    // 0xEX9E: Skips the next instruction if the key stored in VX(only consider the lowest nibble) is pressed (usually the next instruction is a jump to skip a code block). 
                    if (chip8->keypad[chip8->V[chip8->inst.X]]){
                        chip8->PC += 2;
                    }
                    break;
                
                case 0xA1:
                    // 0xEXA1: Skips the next instruction if the key stored in VX(only consider the lowest nibble) is not pressed (usually the next instruction is a jump to skip a code block).
                    if (!chip8->keypad[chip8->V[chip8->inst.X]]){
                        chip8->PC += 2;
                    }
                    break;
                
            }
            break;

        case 0x0F:
            switch(chip8->inst.NN){
                case 0x0A:
                    // 0xFX0A: A key press is awaited, and then stored in VX (blocking operation, all instruction halted until next key event, delay and sound timers should continue processing).
                    bool key_pressed = false; // bool to check if any key is pressed
                    for (uint8_t i = 0; i < sizeof chip8->keypad; i++){
                        if (chip8->keypad[i]){
                            chip8->V[chip8->inst.X] = i; // i = key (offset into the keypad array)
                            key_pressed = true;
                            break; // no longer need to be in loop once key is pressed
                        }
                    }
                    
                    if(!key_pressed){
                        chip8->PC -= 2; // decrement PC to get current opcode and to wait for the current instruction
                    }

                    break;
                
                case 0x1E:
                    // 0xFX1E: Adds VX to I. VF is not affected.
                    chip8->I += chip8->V[chip8->inst.X];
                    break;
                
                case 0x07:
                    // 0xF07: Sets VX to the value of the delay timer
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    break;

                case 0x15:
                    // 0xF15: Sets delay timer to the value of VX
                    chip8->delay_timer = chip8->V[chip8->inst.X];
                    break;

                case 0x18:
                    // 0xF18: Sets sound timer to the value of VX
                    chip8->sound_timer = chip8->V[chip8->inst.X];
                    break;
                
                case 0x29:
                    // 0xF29: Sets I to the location of the sprite for the character in VX
                    chip8->I = chip8->V[chip8->inst.X] * 5; // offset by 5 * character of VX to get the current character
                    break;
                
                case 0x33:
                    // 0xF33: Stores the binary-coded decimal representation of VX, 
                    // with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
                    uint8_t bcd = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I + 2] = bcd % 10;
                    bcd /= 10;
                    
                    chip8->ram[chip8->I + 1] = bcd % 10;
                    bcd /= 10;
                    
                    chip8->ram[chip8->I] = bcd % 10;
                    bcd /= 10;
                    
                    break;

                case 0x55:
                    // 0xF55: Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
                    for(uint8_t i = 0; i <= chip8->inst.X; i++){
                        chip8->ram[chip8->I + i] = chip8->V[i];
                    }

                    break;
                   
                case 0x65:
                    // 0xF65: Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified.
                    for(uint8_t i = 0; i <= chip8->inst.X; i++){
                        chip8->V[i] = chip8->ram[chip8->I + i];
                    }
                    
                    break;

                default:
                    break;
            }
            break;


        default:
            break; // Unimplemented or invalid opcode
    }

}

void update_timers(chip8_t *chip8){
    if(chip8->delay_timer > 0){
        chip8->delay_timer --;
    }
    
    if(chip8->sound_timer > 0){
        chip8->sound_timer --;
        // TODO: Play sound
    }
    else{
        // TODO: Stop playing sound
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

    // Seed RNG
    srand(time(NULL));

    // Main emulator loop
    while(chip8.state != QUIT){
        // Handle user input
        handle_input(&chip8);
        if (chip8.state == PAUSED) continue;

        // Get_Time();
        uint64_t start_frame_time = SDL_GetPerformanceCounter();

        // Emulate CHIP-8 Instructions
        for(uint32_t i = 0; i < config.clock_rate / 60; i++){
            emulate_instruction(&chip8, config);
        }
        // Get_Time() elapsed since last Get_Time();
        uint64_t end_frame_time = SDL_GetPerformanceCounter();

        const double time_elapsed = (double)((end_frame_time - start_frame_time) / 1000) / SDL_GetPerformanceFrequency();

        // Delay for approximately 60hz/60fps (16.67 ms)
        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0); //if 16.67 greater than time elapsed delay for difference otherwise delay for 0 (don't delay for anything)

        // Update window with changes every 60hz
        update_screen(sdl, config, chip8);

        // Update delay and sound timers every 60hz
        update_timers(&chip8);
    }

    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}
