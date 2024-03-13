#include <stdio.h>

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
} ship;

typedef struct projectile
{
    int x;
    int y;
    int dx;
    int dy;
    int orientationX; // orientation might not be required for bullets
    int orientationY;

} bullet;

// buffers to hold the different ship sprites.
int shipUp[][];
int shipUpLeft[][];
int shipLeft[][];
int shipDownLeft[][];
int shipDown[][];
int shipDownRight[][];
int shipRight[][];
int shipUpRight[][];

int titleScreen[][]; // buffer to hold title screen image

int gameState = 0;

#define TITLE_SCREEN 0
#define IN_GAME 1
#define WINNER_A 2
#define WINNER_B 3

void eraseShips(ship *shipA, ship *shipB);

ship shipA, shipB;

int main()
{

    while (gameState = TITLE_SCREEN)
    {
        // hi
    }

    while (gameState = IN_GAME)
    { // gameplay while loop
        // hi

        eraseShips(&shipA, &shipB); // erase previous spaceships
        // erase previous bullets
        // handle bullet collisions (obstacles, or spaceship) + bullet deletion
        // update spaceship health
        // check for winner; if health = 0, clear screen, switch states
        // grab user input (do they wanna move, do they wanna shoot)
        // if user can, and wants to shoot, spawn a bullet
        // queue shooting sound
        // update ship locations & orientations
        // check ship collisions
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
}