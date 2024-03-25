#include <stdlib.h>
#include <stdbool.h>

/*
*** *** ***
MAIN TO DO LIST:

- make code more easier to parse through
    - namely, make the player_keyboard declarations a bit easier to work with
- try to make it so the player controls are held in separate player keyboard objects?
    - options are:
        A: use a single keyboard object, which will handle the entire keyboard space
            - in this case, the division between players will occur at the midpoint of the arrays
                (ie lets say we have 4 inputs per player. the bindings and directions will be 2*4 = 8 indices, first 4 for P1, last 4 for P2)
            - leaning towards this since it makes handling single separate keyboards MUCH easier
            - also, if we manage to have multiple keyboards, it becomes trivial to expand for that
                - simply use a different keyboard_ptr value for the other keyboard address
        B: modify player_keyboard object, as well as how inputs are handled
            - would probably need a significant restructuring of how we poll for keyboard inputs
            - leaning away from this since im not sure how this would fit if i were to go with interrupts as well...

- figure out how to make a simple box for our space ship
    - will be used to test collisions and movement via keyboard
    - probably will be done in a separate project/C file to keep things a bit tidy
*** *** ***
*/

// TIMER DELCARATION
struct timer{
    volatile unsigned int status;
    volatile unsigned int control;
    volatile unsigned int start_low_value;
    volatile unsigned int start_high_value;
    volatile unsigned int count_low;
    volatile unsigned int count_high;
};

// KEYBOARD DECLARATION
/*
PS/2 Port Description (port 1 start: 0xFF200100, port 2 start: 0xFF200108)
for both, it is the data register then the control register
256-byte FIFO
address: Data Register:
bits 0-7 -> data (readable and writeable!)
bit 15 -> R_VALID (if 1, then we get the head of the data FIFO)
bits 16-31 -> R_AVAIL (displays number of entries in the FIFO, reading decreases it by 1)

(address + 4): Control Register
bit 0: RE (enable interrupts for when R_AVAIL > 0, ie when FIFO has data)
bit 8: RI (when interrupt is pending, RI = 1. reset by emptying the FIFO)
bit  10: CE (shows if an error has occurred when sending a command TO the PS/2)
*/

#define inputs_per_player (int) 8
#define BR_CODE (char) 0xF0
#define MULTI_BYTE_CODE (char) 0xE0

struct ps2{
    volatile unsigned char data; // 8 bits of data
    volatile unsigned char R_VALID;
    volatile unsigned short int R_AVAIL;
    volatile unsigned int control;
};

// stores the byte code AND if it is multiple bytes long
// multi_byte key codes will ALWAYS start with E0, then whatever code it is!
typedef struct key_code{
    char byte_code;
    bool multi_byte;
} key_code;

// container for the controls a player may have
typedef struct player_keyboard{
    bool is_breaking;
    bool is_multi_byte;
    key_code bindings[inputs_per_player];
    bool directions[inputs_per_player];
    struct ps2 *keyboard_ptr;
} player_keyboard;

int check_key_direction(player_keyboard *keyboard, char current_input){
    for(int k = 0; k < inputs_per_player; k++){
        // iterate through the key codes, check if the current input matches
        // if it does, check if that key is a multi-byte, and if we are actively looking for an multi byte as well
        if(current_input == keyboard->bindings[k].byte_code){
            // we are touching a key we care about! return its corresponding slot/direction
            if(keyboard->bindings[k].multi_byte == keyboard->is_multi_byte){
                return k;
            }
        }
    }
    // we didnt touch a key we care about
    return -1;
}

void handle_Keyboard(player_keyboard *keyboard, char *current_input){
    if((keyboard->keyboard_ptr)->R_AVAIL){
        // there is data available
        *current_input = keyboard->keyboard_ptr->data;
        
        int key_state;
        if(*current_input == BR_CODE){
            // breaking key start
            keyboard->is_breaking = true;
        }else if(keyboard->is_breaking){
            // breaking key end point, actually "let go" of key
            key_state = check_key_direction(keyboard, *current_input);
            if(key_state != -1){
                keyboard->directions[key_state] = 0;
            }
            keyboard->is_breaking = false;
            keyboard->is_multi_byte = false;
        }else if(*current_input == MULTI_BYTE_CODE){
            // multi key input start!
            keyboard->is_multi_byte = true;
        }else{
            // normal input, check if it is an input we care about
            key_state = check_key_direction(keyboard, *current_input);
            if(key_state != -1){
                keyboard->directions[key_state] = true;
            }
            // we are done looking for a multi-byte entry, we should already be getting one
            keyboard->is_multi_byte = false;
            keyboard->is_breaking = false;
        }
    }
}

struct timer *const timer_ptr = ((struct timer*)0xFF202000);
struct ps2 *const ps2_ptr1 = ((struct ps2*) 0xFF200100);
struct ps2 *const ps2_ptr2 = ((struct ps2*) 0xFF200108);
volatile int *switch_ptr = (int*) (0xFF200040);
volatile int *LEDs = (int*) 0xff200000;
volatile int *HEX_0 = (int*) 0xFF200020;
volatile int *HEX_5 = (int*) 0xFF200030;

typedef struct spaceship
{
    int x;
    int y;
    int dx;
    int dy;
    int orientationX;
    int orientationY;
    int health;
    bool canShoot;
    bool wantShoot;
} ship;

#define ship_size 5



int main(void){
    int LED_Out = 0;

    // PLAYER 1 KEYBOARD SET UP
    const key_code player1_movement_bindings[inputs_per_player];
    bool p1_directions[inputs_per_player];

    player_keyboard p1_keyboard = {
        .is_breaking = 0,
        .is_multi_byte = 0,
        .bindings = {
            {0x1d, false}, // W
            {0x1b, false}, // S
            {0x1c, false}, // A
            {0x23, false}, // D
            {0x75, true}, // UP ARROW
            {0x72, true}, // DOWN ARROW
            {0x6b, true}, // LEFT ARROW
            {0x74, true} // RIGHT ARROW
        },
        .directions = p1_directions,
        .keyboard_ptr = ps2_ptr1
    };
    
    p1_keyboard.keyboard_ptr->data = 0xF4;
    p1_keyboard.keyboard_ptr->data = 0xFF;

    char current_input;

    while(1){
        LED_Out = 0;
        handle_Keyboard(&p1_keyboard, &current_input);
        // output data onto LEDS
        
        for(int k = 0; k < inputs_per_player; k++){
            if(p1_keyboard.directions[k]){
                LED_Out += 0x1 << k;
            }
        }
        *LEDs = LED_Out;
    }

}
