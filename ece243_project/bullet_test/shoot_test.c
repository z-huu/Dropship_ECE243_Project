#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

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

Shooting To Do List:
- use frame counter to determine when you can shoot
    - set int on how much frames to count after firing a shot (fire_rate)
    - each frame after we've shot and are in cooldown, increment frame_count
    - when frame_count == fire_rate, we can shoot again, and poll for input
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

// GLOBALS
int num_bullets = 0;
int frame_count = 0;

#define pixel_step 2

#define x_min_threshold 5
#define y_min_threshold 5
#define x_max_threshold 315
#define y_max_threshold 235

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

#define hitbox_total 1
bool collision_frame[400][300];
bool ring_frame[400][300];
bool game_end = false;

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

typedef struct powerup{
    int x;
    int y;
    int buff_fire_rate;
    int speed_buff;
    int bullet_speed;
    int duration;
    int dmg_buff;
    struct powerup* prev;
    struct powerup* next;
}powerup;

typedef struct powerup_linked_list{
    int num;
    powerup* head;
}powerup_linked_list;

powerup_linked_list powerup_list;

#define powerup_spawn_rate 160
#define powerup_size 5

powerup* create_powerup(){
    //create the powerup
    powerup *new_buff = (powerup*) malloc(sizeof(powerup));
    powerup *current_buff;
    int k = 0;

    if(new_buff){
        powerup_list.head->prev = new_buff;
        new_buff->next = powerup_list.head;
        powerup_list.head = new_buff;
        powerup_list.num++;
        //printf("t = %d. ALLOCATED new buff at addr %d. Next addr %d \n", frame_count, new_buff, new_buff->next);

        // set up

        return new_buff;
    }
    // if here, one of the allocations failed, so give a null ptr
    return NULL;
}

// can certainly be optimized
void destroy_powerup(powerup* deleting_powerup){
    // check if list is empty to begin with
    if(powerup_list.head == NULL){
        return;
    }   

    // avoid freeing NULL address
    if(deleting_powerup == NULL){
        return;
    }

    // check if deleting bullet is the head...
    if(powerup_list.head == deleting_powerup){
        powerup_list.head = deleting_powerup->next;
        //printf("t = %d. FREEING BULLET HEAD at addr %d. \n", frame_count, deleting_bullet);
        //printf("^ Prev Bullet Addr: %d. Next Bullet Addr: %d. \n", frame_count, 0, deleting_bullet->next);
        free(deleting_powerup);
        powerup_list.num--;
        return;
    }

    deleting_powerup->prev->next = deleting_powerup->next;
    deleting_powerup->next->prev = deleting_powerup->prev;

    //printf("t = %d. FREEING BULLET at addr %d.\n", frame_count, deleting_bullet);
    //printf("^ Prev Bullet Addr: %d. Next Bullet Addr: %d.\n", frame_count, deleting_bullet->prev, deleting_bullet->next);
    powerup_list.num--;
    free(deleting_powerup);
    return;
}

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
    
    int fire_rate;
    int frame_count;
    
    int size;
    int input_buffer;
    
    int ring_timer;
    
    int ship_speed;
    int powerup_time;
    int bullet_speed;
    int bullet_dmg;
    int bullet_bounce_count;
    bool canShoot;
} ship;

ship player1_ship;
ship player2_ship;

#define ship_size 5
#define frame_rate 60
#define timer_frequency 100000000

#define base_ship_speed 1
#define base_bullet_speed 1
#define base_fire_rate 30
#define base_bullet_damage 10
#define base_bounce_count 3

#define ship_initial_hp 50
#define OOB_time 300
#define ring_dmg 5
#define ring_rate 30

#define powerup_duration 150
#define buffed_fire_rate 10
#define buffed_ship_speed 4
#define buffed_bullet_speed 15
#define buffed_bullet_dmg 20
#define buffed_bounce_count 6

#define bullet_size 4

bool player_collide(int x_c, int y_c, ship *plr){
    if(x_c >= plr->x && x_c <= plr->x + plr->size){
        if(y_c >= plr->y && y_c <= plr->y + plr->size){
            return true;
        }
    }
    return false;
}

void handle_ship_physics(ship *player_ship){
    // check collisions of ship, stop movement in that direction if it will collide
    // new plan: x direction then y direction move and checks
    
    int x_offset = 0;
    bool x_collide = false;
    bool in_ring = false;

    if(player_ship->dx > 0){
        x_offset = ship_size;
    }else if(player_ship->dx == 0){
        x_collide = true;
    }
    powerup* currentBuff;

    if(player_ship->x + player_ship->ship_speed * player_ship->dx > x_min_threshold 
    && player_ship->x + player_ship->ship_speed * player_ship->dx < x_max_threshold - ship_size - 1){
        // x_c will be the distance we have travelled so far
        for(int x_c = 0; 
        x_c < player_ship->ship_speed && !x_collide; 
        x_c++){
            // scan the ship's height, each horizontal pixel step
            for(int y_c = player_ship->y; 
            y_c < player_ship->y + ship_size; 
            y_c++){

                if(player_ship->x > x_min_threshold
                && player_ship->x < x_max_threshold
                && collision_frame[player_ship->x + x_offset + (player_ship->dx)][y_c]){
                    if(ring_frame[player_ship->x + x_offset + (player_ship->dx)][y_c]){
                        // player in ring, tick downward but let them move normally
                        in_ring = true;
                    }else{
                        x_collide = true;
                        player_ship->x -= player_ship->dx;
                    }

                    // check powerup pickups
                    currentBuff = powerup_list.head;
                    while(currentBuff != NULL){
                        for(int k = 0; k < powerup_size; k++){
                            for(int j = 0; j < powerup_size; j++){
                                if(player_collide(currentBuff->x + k, currentBuff->y + j, player_ship)){
                                    // player has collected powerup, delete it
                                    apply_powerup(currentBuff, player_ship);
                                    destroy_powerup(currentBuff);
                                }
                                currentBuff = currentBuff->next;
                            }
                        }
                        
                    }
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

    if(player_ship->y + player_ship->ship_speed * player_ship->dy > y_min_threshold 
    && player_ship->y + player_ship->ship_speed * player_ship->dy < y_max_threshold - ship_size - 1){
        bool y_collide = false;
        // y_c will be the distance we have travelled so far
        for(int y_c = 0; 
        y_c < player_ship->ship_speed && !y_collide; 
        y_c++){
            // scan the ship's length, each vertical pixel step
            for(int x_c = player_ship->x; 
            x_c < player_ship->x + ship_size; 
            x_c++){
                if(player_ship->y > y_min_threshold
                && player_ship->y < y_max_threshold
                && collision_frame[x_c][player_ship->y + y_offset + (player_ship->dy)]){
                    if(ring_frame[x_c][player_ship->y + y_offset + (player_ship->dy)]){
                        // player in ring, tick down but let them move normally
                        in_ring = true;
                    }else{
                        y_collide = true;
                        player_ship->y -= player_ship->dy;
                    }
                }
            }
            player_ship->y += player_ship->dy;
        }
    }

    // check ring status of player
    if(in_ring){
        player_ship->ring_timer = (player_ship->ring_timer > 0) ? player_ship->ring_timer - 1 : ring_rate;
    }else{
        player_ship->ring_timer += (player_ship->ring_timer < OOB_time) ? 1 : 0;
    }
}

typedef struct bullet{
    int x;
    int y;
    int old_x;
    int old_y;
    int dx;     
    int dy;
    int damage;
    int speed;
    int life_time;
    int size;
    int bounce_count;
    bool wall_pen;
    struct bullet* next;
    struct bullet* prev;
    ship *source_player;
}bullet;

typedef struct bullet_linked_list{
    int num;
    bullet* head;
}bullet_linked_list;

bullet_linked_list bullet_list;

// Inserts bullet into head of linked list
bullet* create_bullet(){
    //create the bullet
    bullet *new_bullet = (bullet*) malloc(sizeof(bullet));
    bullet *current_bullet;
    int k = 0;

    if(new_bullet){
        bullet_list.head->prev = new_bullet;
        new_bullet->next = bullet_list.head;
        bullet_list.head = new_bullet;
        bullet_list.num++;
        num_bullets++;
        //printf("t = %d. ALLOCATED new bullet at addr %d. Next addr %d \n", frame_count, new_bullet, new_bullet->next);

        //current_bullet = bullet_list.head;
        //k = 0;
        //while(current_bullet != NULL){
        //    printf("BULLET #%d POS (%d, %d). ADDR: %d\n", k, current_bullet->x, current_bullet->y, current_bullet);
        //    current_bullet = current_bullet->next;
        //    k++;
        //}
    
        return new_bullet;
    }
    // if here, one of the allocations failed, so give a null ptr
    return NULL;
}

// can certainly be optimized
void destroy_bullet(bullet* deleting_bullet){
    // check if list is empty to begin with
    if(bullet_list.head == NULL){
        return;
    }   

    // avoid freeing NULL address
    if(deleting_bullet == NULL){
        return;
    }

    // check if deleting bullet is the head...
    if(bullet_list.head == deleting_bullet){
        bullet_list.head = deleting_bullet->next;
        //printf("t = %d. FREEING BULLET HEAD at addr %d. \n", frame_count, deleting_bullet);
        //printf("^ Prev Bullet Addr: %d. Next Bullet Addr: %d. \n", frame_count, 0, deleting_bullet->next);
        free(deleting_bullet);
        num_bullets--;
        bullet_list.num--;
        return;
    }

    deleting_bullet->prev->next = deleting_bullet->next;
    deleting_bullet->next->prev = deleting_bullet->prev;

    //printf("t = %d. FREEING BULLET at addr %d.\n", frame_count, deleting_bullet);
    //printf("^ Prev Bullet Addr: %d. Next Bullet Addr: %d.\n", frame_count, deleting_bullet->prev, deleting_bullet->next);
    bullet_list.num--;
    num_bullets--;
    free(deleting_bullet);
    return;
}

int winner = 0;

void damage_player(ship *player, int damage){
    player->health -= damage;
    //printf("^ HIT PLAYER %d\n", ((player == &player1_ship) ? 1 : 2));
    if(player->health < 0){
        if(game_end){

        }else{
            game_end = true;
            if(player == &player1_ship){
                // p1 died
                winner = 2;
            }else{
                // p2 died
                winner = 1;
            } 
            printf("P%d WINS!\n", winner);
        }
        
    }
}

void update_bullets(){
    bullet *current_bullet = bullet_list.head;
    bool is_tail = false;

    while(current_bullet != NULL && !is_tail && winner == 0){
       // move bullet in space
        // X Direction
        int x_offset = 0;
        bool x_collide = false;
        int y_offset = 0;
        bool y_collide = false;
        bool player_hit = false;
        
        current_bullet->old_x = current_bullet->x;
        current_bullet->old_y = current_bullet->y;

        ship *opposing_player = (current_bullet->source_player == &player1_ship) ? &player2_ship : &player1_ship;
        if(current_bullet->dx > 0){
            x_offset = current_bullet->size;
        }

        // HORIZONTAL MOVEMENT
        
        if(current_bullet->x + current_bullet->speed * current_bullet->dx > x_min_threshold 
        && current_bullet->x + current_bullet->speed * current_bullet->dx < x_max_threshold - current_bullet->size){
            // x_c will be the distance we have travelled so far
            for(int x_c = 0; 
            x_c < current_bullet->speed && !x_collide && !player_hit && current_bullet->dx != 0; 
            x_c = x_c + pixel_step){
                // scan the ship's height, each horizontal pixel step
                for(int y_c = current_bullet->y; 
                y_c < current_bullet->y + current_bullet->size && !player_hit && !x_collide; 
                y_c++){
                    // insert player checks here, BEFORE checking wall hit
                    player_hit = player_hit || player_collide(current_bullet->x + x_offset + current_bullet->dx, y_c, opposing_player);
                    if(player_hit){
                        printf("t = %d. PLR HIT X\n", frame_count);
                    }else if(current_bullet->x > x_min_threshold
                            && current_bullet->x < x_max_threshold
                            && collision_frame[current_bullet->x + x_offset + (current_bullet->dx)][y_c]){
                        x_collide = true;
                        current_bullet->x -= current_bullet->dx * pixel_step;
                    }
                }
                current_bullet->x += current_bullet->dx * pixel_step;
            }
        }else{
            x_collide = true;
        }

        // VERTICAL MOVEMENT
        
        if(current_bullet->dy > 0){
            y_offset = current_bullet->size;
        }

        if(current_bullet->y + current_bullet->speed * current_bullet->dy > y_min_threshold 
        && current_bullet->y + current_bullet->speed * current_bullet->dy < y_max_threshold - current_bullet->size){
            // y_c will be the distance we have travelled so far
            for(int y_c = 0; 
            y_c < current_bullet->speed && !y_collide && !player_hit && current_bullet->dy != 0; 
            y_c = y_c + pixel_step){
                // scan the ship's length, each vertical pixel step
                for(int x_c = current_bullet->x; 
                x_c < current_bullet->x + current_bullet->size && !player_hit && !y_collide; 
                x_c++){
                    // insert player checks here, BEFORE checking wall hit.
                    player_hit = player_hit || player_collide(x_c, current_bullet->y + y_offset + current_bullet->dy, opposing_player);
                    if(player_hit){
                        printf("t = %d. PLR HIT Y\n", frame_count);
                    } else if(current_bullet->y > y_min_threshold
                            && current_bullet->y < y_max_threshold
                            && collision_frame[x_c][current_bullet->y + y_offset + (current_bullet->dy)]){
                        y_collide = true;
                        current_bullet->y -= current_bullet->dy * pixel_step;
                    }
                }
                current_bullet->y += current_bullet->dy * pixel_step;
            }
        }else{
            y_collide = true;
        }

        if(player_hit){
            // damage player THEN destroy bullet
            damage_player(opposing_player, current_bullet->damage);
            printf("t = %d: PLAYER HIT: Destroy bullet at addr %d\n", frame_count, current_bullet);
            destroy_bullet(current_bullet);
        }else if(x_collide || y_collide){
            // bullet collided with wall!
            //printf("t = %d. Opp PLR = %d, P1 = %d, P2 = %d\n", frame_count, opposing_player, &player1_ship, &player2_ship);
            if(current_bullet->bounce_count > 0){
                // bounce the bullet based on type of collision
                current_bullet->dx *= (x_collide) ? -1 : 1;
                current_bullet->dy *= (y_collide) ? -1 : 1;
                //printf("Bounce (addr %d): (dx, dy) = (%d, %d). t = %d\n", current_bullet, current_bullet->dx, current_bullet->dy, frame_count);
                current_bullet->bounce_count--;
            }else{
                // no more bounces, delete bullet on collision
                //printf("Collision: Destroy bullet at addr %d, t = %d\n", current_bullet, frame_count);
                destroy_bullet(current_bullet);
            }
            
        }else if(current_bullet->life_time < 0){
            // delete this bullet if its lifetime is out!
            //printf("Lifespan: Destroy bullet at addr %d. t = %d\n", current_bullet, frame_count);
            destroy_bullet(current_bullet);
        }
        //printf("moved bullet at addr %d\n", current_bullet);
        //printf("next bullet is at %d\n", current_bullet->next);
        current_bullet->life_time--;
        if(current_bullet->next == NULL){
            is_tail = true;
        }else{
            current_bullet = current_bullet->next;

        }
    }
    //printf("done bullets\n");
}

const int shoot_buffer = 3;

void shoot(ship *player){
    // take position and orientation data to create a bullet
    // bullet physics functions will handle the rest
    if(player->canShoot){
        if(player->orientationX != 0 || player->orientationY != 0){
            if(player->input_buffer >= shoot_buffer){
                // if we are actually aiming to shoot somewhere
                player->frame_count = 0;
                player->input_buffer = 0;
                player->canShoot = false;
                bullet* new_bullet = create_bullet();
                if(new_bullet){
                    new_bullet->x = player->x;
                    new_bullet->y = player->y;
                    new_bullet->dx = player->orientationX;
                    new_bullet->dy = player->orientationY;
                    new_bullet->damage = player->bullet_dmg;
                    new_bullet->life_time = 120;
                    new_bullet->speed = player->bullet_speed;
                    new_bullet->size = bullet_size;
                    new_bullet->bounce_count = player->bullet_bounce_count;
                    new_bullet->source_player = player;
                }
                

                //printf("t = %d. shooting! %d bullets by P%d atm\n", frame_count, num_bullets, (((player == &player1_ship) ? 1 : 2)));
            }else{
                player->input_buffer++;
            }
        }
    }else{
        // cant shoot, weird scenario made in case
        return;
    }
}

void update_ship_on_frame(ship *player){
    if (player->frame_count >= player->fire_rate)
    {
        // can shoot
        player->canShoot = true;
    }else{
        player->frame_count++;
        player->canShoot = false;
    }
    if(player->powerup_time > 0){
        player->powerup_time--;
    }else{
        player->fire_rate = base_fire_rate;
        player->ship_speed = base_ship_speed;
        player->bullet_speed = base_bullet_speed;
        player->bullet_dmg = base_bullet_damage;
        player->bullet_bounce_count = base_bounce_count;
    }
}

void apply_powerup(powerup *buff, ship *plr){
    plr->powerup_time = buff->duration;
    plr->fire_rate = buff->buff_fire_rate;
    plr->ship_speed = buff->speed_buff;
    plr->bullet_speed = buff->bullet_speed;
    plr->bullet_dmg = buff->dmg_buff;
}

#define shrink_hori_rate 4
#define shrink_vert_rate 3
#define shrink_delay 20
#define minimum_ring_x 80
#define minimum_ring_y 60
#define ring_initial_delay 120

int ring_x = 320/2;
int ring_y = 240/2;
int ring_center_x = 320/2; // to be randomized to be centered at a random pt, b/n 110-230 (ie midpt = 170, range of 60)
int ring_center_y = 240/2; // to be randomized to be centered at a random pt, b/n 80-160 (midpt = 120, range of 40)


void ring_update(){
    if(frame_count > ring_initial_delay && (frame_count%shrink_delay) == 0){
        if(!(ring_x <= minimum_ring_x && ring_y <= minimum_ring_y)){
            //printf("t = %d: SHRINKING RING. Size: (%d, %d) Center: (%d, %d).\n", frame_count, ring_x, ring_y, ring_center_x, ring_center_y);
            // shrink ring now
            // NOTE
            // currently shrinking in a box like shape
            // horizontal shrink rate determined by shrink_hori_rate
            // vertical shrink rate determined by shrink_vert_rate
            ring_x = (ring_x - shrink_hori_rate > minimum_ring_x) ? ring_x - shrink_hori_rate : minimum_ring_x;
            ring_y = (ring_y - shrink_vert_rate > minimum_ring_y) ? ring_y - shrink_vert_rate : minimum_ring_y;
            // apply ring changes onto collision frame!
            for(int x_c = 0; x_c < VGA_x; x_c++){
                for(int y_c = 0; y_c < VGA_y; y_c++){
                    // if the coordinate is OUTSIDE of ring, set to true!
                    if(x_c < (ring_center_x - ring_x) || x_c > (ring_center_x + ring_x)){
                        collision_frame[x_c][y_c] = true;
                        ring_frame[x_c][y_c] = true;
                    }
                    if(y_c < (ring_center_y - ring_y) || y_c > (ring_center_x + ring_y)){
                        // out of ring, set to true!
                        collision_frame[x_c][y_c] = true;
                        ring_frame[x_c][y_c] = true;
                    }
                }
            }
        }
    }
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
	*(pixel_ctrl_ptr + 1) = (int)&Buffer1;
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);	 // we draw on the back buffer
	clear_screen();	 // pixel_buffer_start points to the pixel buffer

	const unsigned short int color_list[] = {0xffff, 0xf800, 0x07e0, 0x001f,
											 0xf0b0, 0x0f0b, 0xf22f, 0x2ff2,
											 0x22ff, 0xff22};

	struct hitbox colliders[hitbox_total];

	// randomize locations of hitboxes and their sizes
	for (int k = 0; k < hitbox_total; k++) {
		colliders[k].x1 = 55 + 35*(rand()%3);
		colliders[k].y1 = 55 + 30*(rand()%3);

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
    bool p1_directions[inputs_per_player];
    bool p2_directions[inputs_per_player];

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

    player_keyboard p2_keyboard = {
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
        .directions = p2_directions,
        .keyboard_ptr = ps2_ptr2
    };
    p2_keyboard.keyboard_ptr->data = 0xF4;
    p2_keyboard.keyboard_ptr->data = 0xFF;

    char current_input_p1;
    char current_input_p2;

    // TIMER SET UP, may not be needed...
    timer_ptr->start_low_value = (timer_frequency/frame_rate);
    timer_ptr->start_high_value = ((timer_frequency/frame_rate) >> 16);
    timer_ptr->control = 0b0110;

    player1_ship.x = 50;
    player1_ship.y = 50;
    player1_ship.frame_count = 0;
    player1_ship.canShoot = false;
    player1_ship.size = ship_size;
    player1_ship.input_buffer = 0;
    player1_ship.ring_timer = OOB_time;
    player1_ship.health = ship_initial_hp;

    player1_ship.fire_rate = base_fire_rate;
    player1_ship.ship_speed = base_ship_speed;
    player1_ship.bullet_bounce_count = base_bounce_count;
    player1_ship.powerup_time = 0;
    player1_ship.bullet_dmg = base_bullet_damage;
    player1_ship.bullet_speed = base_bullet_speed;

    player2_ship.x = 150;
    player2_ship.y = 150;
    player2_ship.frame_count = 0;
    player2_ship.canShoot = false;
    player2_ship.size = ship_size;
    player2_ship.input_buffer = 0;
    player2_ship.ring_timer = OOB_time;
    player2_ship.health = ship_initial_hp;

    player2_ship.fire_rate = base_fire_rate;
    player2_ship.ship_speed = base_ship_speed;
    player2_ship.bullet_bounce_count = base_bounce_count;
    player2_ship.powerup_time = 0;
    player2_ship.bullet_dmg = base_bullet_damage;
    player2_ship.bullet_speed = base_bullet_speed;

    int h_drawn = 0;
    
    ring_center_x = (rand() % 60 - 30) + VGA_x/2;
    ring_center_y = (rand() % 40 - 20) + VGA_y/2;

    // BULLET DYNAMIC ARRAY SETUP    
    printf("%d", sizeof(bullet));
    bullet *current_bullet;
    while(1){
        LED_Out = 0;
        handle_Keyboard(&p1_keyboard, &current_input_p1);
        handle_Keyboard(&p2_keyboard, &current_input_p2);
        // RENDER STEP
        // wait for vsync and update buffer positions
		wait_for_vsync();
		pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        frame_count++;
        // VGA ERASING OLD POSITIONS
		// erase old boxes
		draw_box(player1_ship.old_x, player1_ship.old_y, player1_ship.size, player1_ship.size, 0);
		draw_box(player2_ship.old_x, player2_ship.old_y, player2_ship.size, player2_ship.size, 0);
        
        // drawing hitboxes
        if(h_drawn < 2){
           		// draw hitboxes
            for (int k = 0; k < hitbox_total; k++) {
			    draw_box(colliders[k].x1, colliders[k].y1,
					 colliders[k].x2 - colliders[k].x1,
					 colliders[k].y2 - colliders[k].y1, color_list[1]);
		    } 
            h_drawn++;
        }


        //erase old bullets
        current_bullet = bullet_list.head;
        while(current_bullet != NULL){
            draw_box(current_bullet->x - current_bullet->dx *current_bullet->speed, 
                    current_bullet->y - current_bullet->dy * current_bullet->speed,
                    current_bullet->size, current_bullet->size, 0);
            current_bullet = current_bullet->next;
        }


        // PHYSICS STEPPING
        player1_ship.dy = p1_keyboard.directions[1] - p1_keyboard.directions[0];
        player1_ship.dx = p1_keyboard.directions[3] - p1_keyboard.directions[2];
        player1_ship.orientationY = p1_keyboard.directions[5] - p1_keyboard.directions[4];
        player1_ship.orientationX = p1_keyboard.directions[7] - p1_keyboard.directions[6];
        
        player2_ship.dy = p2_keyboard.directions[1] - p2_keyboard.directions[0];
        player2_ship.dx = p2_keyboard.directions[3] - p2_keyboard.directions[2];
        player2_ship.orientationY = p2_keyboard.directions[5] - p2_keyboard.directions[4];
        player2_ship.orientationX = p2_keyboard.directions[7] - p2_keyboard.directions[6];
        
        handle_ship_physics(&player1_ship);
        handle_ship_physics(&player2_ship);

        // handle shoot cooldown and whether or not we have fired
        update_ship_on_frame(&player1_ship);
        update_ship_on_frame(&player2_ship);

        if(player1_ship.ring_timer <= 0){
            damage_player(&player1_ship, ring_dmg);
            printf("t = %d. BURN P1\n", frame_count);
        }
        if(player2_ship.ring_timer <= 0){
            damage_player(&player2_ship, ring_dmg);
            printf("t = %d BURN P2\n", frame_count);
        }

        if(player1_ship.canShoot){
            shoot(&player1_ship);
        }
        if(player2_ship.canShoot){
            shoot(&player2_ship);
        }

        update_bullets();
        ring_update();

        // VGA DRAWING UPDATED POSITIONS
        // draw out updated ring!
        if(frame_count > ring_initial_delay && (frame_count % shrink_delay == 0 || frame_count % shrink_delay == 1 )){
            if(ring_x <= minimum_ring_x && ring_y <= minimum_ring_y){

            }else{
                // ring has changed!
                
                for(int x_c = ring_center_x - ring_x - shrink_hori_rate; x_c < ring_center_x + ring_x + shrink_hori_rate; x_c++){
                    if(ring_center_y - ring_y > 0){
                        plot_pixel(x_c, ring_center_y - ring_y, color_list[6]);
                    }else{
                        plot_pixel(x_c, 0, color_list[6]); 
                    }
                    if(ring_center_y + ring_y < VGA_y){
                        plot_pixel(x_c, ring_center_y + ring_y, color_list[6]);
                    }else{
                        plot_pixel(x_c, y_max_threshold, color_list[6]);
                    }
                }

                for(int y_c = ring_center_y - ring_y - shrink_vert_rate; y_c < ring_center_y + ring_y + shrink_vert_rate; y_c++){
                    if(ring_center_x - ring_x > 0){
                        plot_pixel(ring_center_x - ring_x, y_c, color_list[6]);
                    }else{
                        plot_pixel(0, y_c, color_list[6]);
                    }
                    if(ring_center_x + ring_x < VGA_x){
                        plot_pixel(ring_center_x + ring_x, y_c, color_list[6]);
                    }else{
                        plot_pixel(x_max_threshold, y_c, color_list[6]);
                    }
                }
            }
        }
        // drawing bullets out

        current_bullet = bullet_list.head;
        while(current_bullet != NULL){
            draw_box(current_bullet->x - current_bullet->dx *current_bullet->speed, 
                    current_bullet->y - current_bullet->dy * current_bullet->speed,
                    current_bullet->size, current_bullet->size, color_list[6]);
            current_bullet = current_bullet->next;
        }

        player1_ship.old_x = player1_ship.x;
        player1_ship.old_y = player1_ship.y;
         
        player2_ship.old_x = player2_ship.x;
        player2_ship.old_y = player2_ship.y;

		// draw new boxes at shifted positions
        //int colour_index = abs(player1_ship.orientationX) + abs((player1_ship.orientationY << 1)) + 1;
		draw_box(player1_ship.x, player1_ship.y, player1_ship.size, player1_ship.size, color_list[2]);
        draw_box(player2_ship.x, player2_ship.y, player2_ship.size, player2_ship.size, color_list[7]);
        // output data onto LEDS
        for(int k = 0; k < inputs_per_player; k++){
            if(p1_keyboard.directions[k]){
                LED_Out += 0x1 << k;
            }
        }
        if(player1_ship.canShoot){
            LED_Out += 0x100;
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
