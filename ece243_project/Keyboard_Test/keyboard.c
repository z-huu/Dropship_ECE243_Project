#include <stdlib.h>
#include <stdbool.h>

#define RESOLUTION_X 320
#define RESOLUTION_Y 240
#define BULLET_SPEED 5 // subject to change

/*
TO DO:

- make code more easier to parse through
    - namely, make the player_keyboard declarations a bit easier to work with
- double check if inputs work as desired, EVEN with more complicated keys
    - namely, arrow keys have longer key codes (Up Arrow Key -> E0, 75 Make Code and E0 F0 75 Break Code)
    - cant just take last digit, Numpad 8 also uses 75
    - bright side is E0 ALWAYS signifies a longer make or break code
    - problem is distinguishing betweeen one byte and two byte make codes
    -* maybe make a struct of key objects?
        - stores its make code, break code, and its length (excluding F0 in break code)
        - keys with multi-byte make codes have an array?
        - if we get E0, we know we are looking for a multi byte code
            - hence, if the key_object has is only 1 byte long, we can now ignore it
            - meanwhile, if the key_object is not, we look at the other byte now
        - THIS IDEA WILL REQUIRE QUITE A BIT OF RESTRUCTURING!!!
        
- figure out how to make a simple box for our space ship
    - will be used to test collisions and movement via keyboard

*/

// VGA DECLARATIONS
volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];	  // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

const int VGA_x = 320;
const int VGA_y = 240;

const int x_min = 0;
const int x_max = 320;
const int y_min = 0;
const int y_max = 240;
const int speed = 3;

void clear_screen();

void draw_line(int x0, int y0, int x1, int x2, short int line_colour);

void swap(int* a, int* b);

int poll_status(volatile int* status_reg);

void initializeDirection(int* arr, int size);

void initializeColour(int* arr, int size);

void initializePosition(int* arr, int size, int range);

void wait_for_vsync();

void draw_frame(int* x_box, int* y_box, short int* boxColour, int N);

void handle_physics(int* x_box, int* y_box, int* dx, int* dy, int N, int speed,
					short int** collision_frame);

void draw_box(int x, int y, int x_size, int y_size, short int color);

// AUDIO DECLARATION
// from lecture, trying out for neater code
struct audio_t {
	volatile unsigned int control;
	volatile unsigned char rarc;
	volatile unsigned char ralc;
	volatile unsigned char warc;
	volatile unsigned char walc;
    volatile unsigned int ldata;
	volatile unsigned int rdata;
};

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

struct ps2{
    volatile unsigned char data; // 8 bits of data
    volatile unsigned char R_VALID;
    volatile unsigned short int R_AVAIL;
    volatile unsigned int control;
};

typedef struct player_keyboard{
    char current_input;
    bool breaking;
    const char bindings[4];
    char directions[4];
    struct ps2 *keyboard_ptr;
} player_keyboard;



const char BR_CODE = 0xF0;

int check_key_direction(player_keyboard *keyboard, short int key){
    for(int k = 0; k < 4; k++){
        if(key == keyboard->bindings[k]){
            // we are touching a key we care about! return its corresponding slot/direction
            return k;
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
            keyboard->breaking = true;
        }else if(keyboard->breaking){
            // breaking key end point, actually "let go" of key
            key_state = check_key_direction(keyboard, *current_input);
            if(key_state != -1){
                keyboard->directions[key_state] = 0;
            }
            
            keyboard->breaking = 0;
        }else{
            // normal input, check if it is an input we care about
            key_state = check_key_direction(keyboard, *current_input);
            if(key_state != -1){
                keyboard->directions[key_state] = 1;
            }
        }
    }
}

struct audio_t *const audio_ptr = ((struct audio_t*)0xff203040);
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
    // SETTING UP VGA
    volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
	// declare other variables(not shown)
	// initialize location and direction of rectangles(not shown)

	/* set front pixel buffer to Buffer 1 */
	*(pixel_ctrl_ptr + 1) = (int)&Buffer1;	
    // first store the address in the  back buffer
	/* now, swap the front/back buffers, to set the front buffer location */
	//wait_for_vsync();

	/* initialize a pointer to the pixel buffer, used by drawing functions */
	pixel_buffer_start = *pixel_ctrl_ptr;
	//clear_screen();	 // pixel_buffer_start points to the pixel buffer

	/* set back pixel buffer to Buffer 2 */
	*(pixel_ctrl_ptr + 1) = (int)&Buffer2;
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);	 // we draw on the back buffer
	//clear_screen();	 // pixel_buffer_start points to the pixel buffer

	const unsigned short int color_list[] = {0xffff, 0xf800, 0x07e0, 0x001f,
											 0xf0b0, 0x0f0b, 0xf22f, 0x2ff2,
											 0x22ff, 0xff22};

    int LED_Out = 0;
    const char player1_movement_bindings[4] = {
        0x1D, // W
        0x1B, // S
        0x1C, // A
        0x23 // D
    };
    char p1_directions[4];

    player_keyboard p1_keyboard = {
        .current_input = 0,
        .breaking = 0,
        .bindings = {
                0x1D, // W
                0x1B, // S
                0x1C, // A
                0x23 // D
            },
        .directions = p1_directions,
        .keyboard_ptr = ps2_ptr1
    };
    
    p1_keyboard.keyboard_ptr->data = 0xF4;
    p1_keyboard.keyboard_ptr->data = 0xFF;

    char current_input;

    ship player1;
    player1.x = 50;
    player1.y = 50;
    player1.orientationX = 1;
    player1.orientationY = 1;


    while(1){
        LED_Out = 0;
        handle_Keyboard(&p1_keyboard, &current_input);
        
        // output data onto LEDS
        
        for(int k = 0; k < 4; k++){
            if(p1_keyboard.directions[k] == 1){
                LED_Out += 0x1 << k;
            }
        }
        *LEDs = LED_Out + 0x300;
    }

}

void draw_box(int x, int y, int x_size, int y_size, short int color) {
	for (int a = x; a < x + x_size + 1; a++) {
		for (int b = y; b < y + y_size + 1; b++) {
			plot_pixel(a, b, color);
		}
	}
}

void wait_for_vsync() {
	volatile int* pixel_ctrl_ptr = (int*)0xff203020;
	int status;
	*pixel_ctrl_ptr = 1;
	status = *(pixel_ctrl_ptr + 3);
	while ((status & 0b01) != 0) {
		status = *(pixel_ctrl_ptr + 3);
	}
	// should exit when vsync occurs!
}

int poll_status(volatile int* status_reg) {
	// extract S bit (bit 0 of status reg)
	return *(status_reg) & 0b01;
}

void clear_screen() {
	for (int x = 0; x < VGA_x; x++) {
		for (int y = 0; y < VGA_y; y++) {
			plot_pixel(x, y, 0x0);
		}
	}
}

// largely from lab document
void draw_line(int x0, int y0, int x1, int y1, short int line_colour) {
	int is_steep = abs(y1 - y0) > abs(x1 - x0);

	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int deltaX = x1 - x0;
	int deltaY = abs(y1 - y0);
	int error = -(deltaX / 2);
	int y = y0;
	int y_step;
	if (y0 < y1) {
		y_step = 1;
	} else {
		y_step = -1;
	}

	for (int x = x0; x < x1; x++) {
		if (is_steep) {
			plot_pixel(y, x, line_colour);
		} else {
			plot_pixel(x, y, line_colour);
		}
		error = error + deltaY;
		if (error > 0) {
			y = y + y_step;
			error = error - deltaX;
		}
	}
}

void swap(int* a, int* b) {
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

void plot_pixel(int x, int y, short int line_color) {
	volatile short int* one_pixel_address;

	one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);

	*one_pixel_address = line_color;
}