#include <stdio.h>
#include <stdbool.h>

#define RESOLUTION_X 320
#define RESOLUTION_Y 240
#define BULLET_SPEED 5 // subject to change

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
    bool wantShoot; // maybe redundant

    int movementDirection[4];       // the current user input
    int shootDirection[4];

    int oldx;       // to store the two frame old data for deletion
    int oldy;
    int prevx;
    int prevy;
} ship;

typedef struct projectile
{
    int x;
    int y;
    int dx;
    int dy;
    // circle shaped bullets; no orientation needed

    int oldx;       // to store two frame old data
    int oldy;
    int prevx;
    int prevy;

    int endFrame;       // to keep track of when the bullet is deleted
                        // so we can still delete the past 2 frames
} bullet;

// buffers to hold the different ship sprites.
    // In spriteTest.c 

int titleScreen[][]; // buffer to hold title screen image
int mapLayout[][][]; // buffer to hold the map layout aka obstacles
                     //  index one for which map layout we're using,
                     //  second & third for obstacle coordinates
int gameState = 0;
int currentMap = 0; // by default, use the first map layout

int direction_P1[4]; // arrays to hold current player movement inputs
int direction_P2[4]; // up down left right in THAT ORDER
                     // 0  1    2    3
                     // Put these as a member of the spaceship struct? $$$$$$$

int frameCounter = 0;

short int Buffer1[240][512]; // 240 rows, 320 columns + paddings
short int Buffer2[240][512];

#define TITLE_SCREEN 0
#define IN_GAME 1
#define WINNER_A 2
#define WINNER_B 3

/* Cyclone V FPGA devices */
#define LEDR_BASE 0xFF200000
#define HEX3_HEX0_BASE 0xFF200020
#define HEX5_HEX4_BASE 0xFF200030
#define SW_BASE 0xFF200040
#define KEY_BASE 0xFF200050
#define TIMER_BASE 0xFF202000
#define PIXEL_BUF_CTRL_BASE 0xFF203020
#define CHAR_BUF_CTRL_BASE 0xFF203030

void eraseShips(ship *shipA, ship *shipB);
void eraseOldBullets(bullet **bulletArray);
void handleCollisions(bullet *bulletArray, int ****mapLayout, ship *shipA, ship *shipB, int currentMap);
void checkWinner(ship *shipA, ship *shipB);
void updateShips(ship *shipA, ship *shipB, int* direction_P1, int* direction_P2);
void wait_for_vsync();
void clear_screen();
void drawBullet(int x, int y);
ship *shipA;
ship *shipB;          // ship pointers

bullet bulletArray[100]; // array to keep track of all bullet objects
                         // 100 for now, not sure how many we're going to fire
int numBullets = 0;

int main()
{
    // Initialize double buffering

    volatile int * pixel_ctrl_ptr = (int *)PIXEL_BUF_CTRL_BASE;
    int pixel_buffer_start;
    /* set front pixel buffer to Buffer 1 */
    *(pixel_ctrl_ptr + 1) = (int) &Buffer1; // first store the address in the  back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer

    /* set back pixel buffer to Buffer 2 */
    *(pixel_ctrl_ptr + 1) = (int) &Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
    clear_screen(); // pixel_buffer_start points to the pixel buffer

    while (gameState = TITLE_SCREEN)
    {
        // hi

        // draw map layout (obstacles) before we go into gameplay loop
    }

    while (gameState = IN_GAME)
    { // gameplay while loop
        // hi

        eraseShips(shipA, shipB);     // erase previous spaceships
        eraseOldBullets(bulletArray); // erase previous bullets

        // handle bullet collisions (obstacles, or spaceship) + bullet deletion
        handleCollisions(bulletArray, mapLayout, shipA, shipB, currentMap);
        // update spaceship health

        checkWinner(shipA, shipB); // check for winner; if health = 0, clear screen, switch states

        // poll user input (direction array, do they wanna shoot)
        // update ship locations & orientations, old frame data
        // check ship collisions
        updateShips(shipA, shipB, direction_P1, direction_P2);

        // if user can, and wants to shoot, spawn a bullet
        // queue shooting sound

        // draw health bars, spaceships, bullets
    }

    while (gameState = WINNER_A)
    {
        // hi
    }

    while (gameState = WINNER_B)
    {
        // hi
    }
    frameCounter++;
}

void drawBullet(int x, int y) {

    // Implemented in spriteTest.c
}