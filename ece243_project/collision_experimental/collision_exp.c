#include <stdlib.h>
#include <stdbool.h>

/*
*** *** ***
MAIN TO DO LIST:

1: figure out how to implement keyboard with interrupts
    - the main structure should be pretty similar, just with setting current input via the interrupt, and handling input during the interrupt

2: try to make it so the player controls are held in separate player keyboard objects?
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

3: figure out how to make a simple box for our space ship
    - essentially, on a frame (1/60 s), we move the ship by speed*dx + speed*dy in x and y directions respectively
    - would be good to use hex display numbers?
    - maybe figure out how to display numbers on VGA? (may be overkill for just testing though)
    - will be used to test collisions and movement via keyboard
    - probably will be done in a separate project/C file to keep things a bit tidy

IDEAS FOR PHYSICS:
- maybe have a physics struct, which has points to a large amount of physics based stuff
    - for example, has pointers to the space ships (in an array), array containing all the projectiles, and contains the collision mask as well

CURRENT BUGS:
- left and top boundaries cause weird slowing effect
- similar slowing effect when colliding with intended collision hitbox
- conclusion: collision detection in those scenarios is causing the player to move one unit
- should probably double check how i am checking for collisions again
*** *** ***
*/

// VGA DECLARATION
volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];	  // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

const int VGA_x = 320;
const int VGA_y = 240;

const int x_min = 0;
const int x_max = 320;
const int y_min = 0;
const int y_max = 240;
const int box_size = 2;
const int N = 8;
const int speed = 3;

void clear_screen();

void plot_pixel(int x, int y, short int line_color);

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


// TIMER DECLARATION
struct timer{
    volatile unsigned int status;
    volatile unsigned int control;
    volatile unsigned int start_low_value;
    volatile unsigned int start_high_value;
    volatile unsigned int count_low;
    volatile unsigned int count_high;
};

// hitbox object, describes top left (x1, y1) and bottom right (x2, y2) corners,
// rest can be determined implicitly TO DO: maybe add rotation value? can be
// used to have angled corners/boxes
typedef struct hitbox {
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
} hitbox;

#define hitbox_total 3
bool collision_frame[400][300];

/* KEYBOARD DECLARATION
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
    int old_x;
    int old_y;
    int dx;
    int dy;
    int orientationX;
    int orientationY;
    int health;
    bool canShoot;
    bool wantShoot;
} ship;

#define ship_size 5
#define frame_rate 60
#define timer_frequency 100000000
#define ship_speed 2
#define x_min_threshold 5
#define y_min_threshold 5
#define x_max_threshold 315
#define y_max_threshold 235

typedef struct physics_holder{
    ship *ship_array;
    int ship_num;
    hitbox *hitbox_array;
    int hitbox_num;
};


void handle_ship_physics(ship *player_ship){
    // check collisions of ship, stop movement in that direction if it will collide
    // new plan: x direction then y direction move and checks
    
    int x_offset = 0;
    bool x_collide = false;
    if(player_ship->dx > 0){
        x_offset = ship_size;
    }else if(player_ship->dx == 0){
        x_collide = true;
    }
    
    if(player_ship->x + ship_speed * player_ship->dx > x_min_threshold 
    && player_ship->x + ship_speed * player_ship->dx < x_max_threshold - ship_size - 1){
        // x_c will be the distance we have travelled so far
        for(int x_c = 0; 
        x_c < ship_speed && !x_collide; 
        x_c++){
            // scan the ship's height, each horizontal pixel step
            for(int y_c = player_ship->y; 
            y_c < player_ship->y + ship_size; 
            y_c++){

                if(player_ship->x > x_min_threshold
                && player_ship->x < x_max_threshold
                && collision_frame[player_ship->x + x_offset + (player_ship->dx)][y_c]){
                    x_collide = true;
                    player_ship->x -= player_ship->dx;
                }
            }
            player_ship->x += player_ship->dx;
        }
    }

    int y_offset = 0;
    bool y_collide = false;
    if(player_ship->dy > 0){
        y_offset = ship_size;
    }else if(player_ship->dy == 0){
        y_collide = true;
    }

    if(player_ship->y + ship_speed * player_ship->dy > y_min_threshold 
    && player_ship->y + ship_speed * player_ship->dy < y_max_threshold - ship_size - 1){
        bool y_collide = false;
        // y_c will be the distance we have travelled so far
        for(int y_c = 0; 
        y_c < ship_speed && !y_collide; 
        y_c++){
            // scan the ship's length, each vertical pixel step
            for(int x_c = player_ship->x; 
            x_c < player_ship->x + ship_size; 
            x_c++){
                if(player_ship->y > y_min_threshold
                && player_ship->y < y_max_threshold
                && collision_frame[x_c][player_ship->y + y_offset + (player_ship->dy)]){
                    y_collide = true;
                    player_ship->y -= player_ship->dy;
                }
            }
            player_ship->y += player_ship->dy;
        }
    }
    /*
    if(player_ship->x + ship_speed * player_ship->dx > x_min && 
    player_ship->x + ship_speed * player_ship->dx < x_max - ship_size - 1){
        // within bounds, move in X direction
        // fire vertical raycasts horizontally to the ship to determine horizontal collisions
        // can increment by larger amounts to make less precise but faster
        // left then right collision checks

        
        int x_dist = 0;
        bool x_collide = false;
        for(x_dist = 0; x_dist <= ship_speed && x_collide == false; x_dist++){
            for(int k = player_ship->y + 1; k < player_ship->y + ship_size - 1; k++){
                if(player_ship->x + x_dist * player_ship->dx < x_min_threshold || 
                player_ship->x + x_dist * player_ship->dx > x_max_threshold ||
                collision_frame[player_ship->x + x_dist * player_ship->dx][k] ||
                collision_frame[player_ship->x + x_dist * player_ship->dx + ship_size][k]
                ){
                    // collision!
                    x_collide = true;
                    //player_ship->x = player_ship->x - player_ship->dx;
                }else{
                    
                }
            }
            player_ship->x = player_ship->x + player_ship->dx;
        }
        //player_ship->x = player_ship->x + (x_dist) * player_ship->dx;
    }
    
    if(player_ship->y + ship_speed * player_ship->dy > y_min &&
    player_ship->y + ship_speed * player_ship->dy < y_max - ship_size - 1){
        // within bounds, move in Y direction
        // fire horizontal raycasts vertically to determine horizontal collisions
        // can increment by larger amounts to make less precise but faster
        // left then right collision checks
        int y_dist = 0;
        bool y_collide = false;
        for(y_dist = 0; y_dist <= ship_speed && y_collide == false; y_dist++){
            for(int k = player_ship->x + 1; k < player_ship->x + ship_size - 1; k++){
                if(player_ship->y + y_dist * player_ship->dx < y_min_threshold ||
                player_ship->y + y_dist * player_ship->dx > y_max_threshold ||
                collision_frame[k][player_ship->y + y_dist * player_ship->dy] ||
                collision_frame[k][player_ship->y + y_dist * player_ship->dy + ship_size]
                ){
                    // collision!
                    y_collide = true;
                }else{
                }
            }
            player_ship->y = player_ship->y + player_ship->dy;
        }
        //player_ship->y = player_ship->y + (y_dist) * player_ship->dy;
    }
    */
}


int main(void){
    // VGA SET UP
    	volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
	// declare other variables(not shown)
	// initialize location and direction of rectangles(not shown)

	/* set front pixel buffer to Buffer 1 */
	*(pixel_ctrl_ptr + 1) =
		(int)&Buffer1;	// first store the address in the  back buffer
	/* now, swap the front/back buffers, to set the front buffer location */
	wait_for_vsync();

	/* initialize a pointer to the pixel buffer, used by drawing functions */
	pixel_buffer_start = *pixel_ctrl_ptr;
	clear_screen();	 // pixel_buffer_start points to the pixel buffer

	/* set back pixel buffer to Buffer 2 */
	*(pixel_ctrl_ptr + 1) = (int)&Buffer2;
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);	 // we draw on the back buffer
	clear_screen();	 // pixel_buffer_start points to the pixel buffer

	const unsigned short int color_list[] = {0xffff, 0xf800, 0x07e0, 0x001f,
											 0xf0b0, 0x0f0b, 0xf22f, 0x2ff2,
											 0x22ff, 0xff22};

	struct hitbox colliders[hitbox_total];

	// randomize locations of hitboxes and their sizes
	for (int k = 0; k < hitbox_total; k++) {
		colliders[k].x1 = 5 + 35*k;
		colliders[k].y1 = 5 + 30*k;

		do {
		colliders[k].x2 = rand() % x_max -1;
		colliders[k].y2 = rand() % y_max -1;
		} while (colliders[k].x2 < colliders[k].x1 || colliders[k].y2 < colliders[k].y1);

        /*	Print statements to help CPUlator debugging
        * printf("=========================\n");
		* printf("Collider %d x1: %d y1: %d\n", k, colliders[k].x1, colliders[k].y1);
		* printf("Collider %d x2: %d y2: %d\n", k, colliders[k].x2, colliders[k].y2);
        */
    
    }

	// fill in collision mask to show where players CANNOT exist in
	for (int k = 0; k < hitbox_total; k++) {
		for (int x_coord = colliders[k].x1; x_coord < colliders[k].x2;
			 x_coord++) {
			for (int y_coord = colliders[k].y1; y_coord < colliders[k].y2;
				 y_coord++) {
				collision_frame[x_coord][y_coord] = true;
			}
		}
	}

    for(int k = 0; k < VGA_x; k++){
        for(int j = 0; j < VGA_y; j++){
            if(k < x_min_threshold || k > x_max_threshold){
                collision_frame[k][j] = true;
            }
            if(j < y_min_threshold || j > y_max_threshold){
                collision_frame[k][j] = true;
            }
        }
    }
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

    // TIMER SET UP
    timer_ptr->start_low_value = (timer_frequency/frame_rate);
    timer_ptr->start_high_value = ((timer_frequency/frame_rate) >> 16);
    timer_ptr->control = 0b0110;

    ship player1_ship;
    player1_ship.x = 50;
    player1_ship.y = 50;
    
    int h_drawn = 0;
    
    while(1){
        LED_Out = 0;
        handle_Keyboard(&p1_keyboard, &current_input);
        
        //Physics Step
        /*
        if(timer_ptr->status & 0b1){
            // Physics Timer Finished, handle physics now
            timer_ptr->status = 0b0;
            LED_Out = LED_Out ^ 0b100000000;
            player1_ship.dy = p1_directions[0] - p1_directions[1];
            player1_ship.dx = p1_directions[2] - p1_directions[3];
            player1_ship.orientationY = p1_directions[4] - p1_directions[5];
            player1_ship.orientationX = p1_directions[6] - p1_directions[7];
            handle_ship_physics(&player1_ship);
        }
        */

        player1_ship.dy = p1_keyboard.directions[1] - p1_keyboard.directions[0];
        player1_ship.dx = p1_keyboard.directions[3] - p1_keyboard.directions[2];
        handle_ship_physics(&player1_ship);
        
        // RENDER STEP
        // wait for vsync and update buffer positions
		wait_for_vsync();
		pixel_buffer_start = *(pixel_ctrl_ptr + 1);

		// erase old boxes
		draw_box(player1_ship.old_x, player1_ship.old_y, ship_size, ship_size, 0);
        
        if(h_drawn < 2){
           		// draw hitboxes
            for (int k = 0; k < hitbox_total; k++) {
			draw_box(colliders[k].x1, colliders[k].y1,
					 colliders[k].x2 - colliders[k].x1,
					 colliders[k].y2 - colliders[k].y1, color_list[1]);
		    } 
            h_drawn++;
        }


        player1_ship.old_x = player1_ship.x;
        player1_ship.old_y = player1_ship.y;

		// draw new boxes at shifted positions
        //int colour_index = abs(player1_ship.orientationX) + abs((player1_ship.orientationY << 1)) + 1;
		draw_box(player1_ship.x, player1_ship.y, ship_size, ship_size, color_list[2]);

        // output data onto LEDS
        for(int k = 0; k < inputs_per_player; k++){
            if(p1_keyboard.directions[k]){
                LED_Out += 0x1 << k;
            }
        }
        *LEDs = LED_Out;
    }
}

// VGA FUNCTION DEFINITIONS
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

	one_pixel_address = (int) pixel_buffer_start + (y << 10) + (x << 1);

	*one_pixel_address = line_color;
}
