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
    bool wantShoot;
} ship;

typedef struct projectile
{
    int x;
    int y;
    int dx;
    int dy;
    // circle shaped bullets; no orientation needed

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
int mapLayout[][][];   //buffer to hold the map layout aka obstacles
                       // index one for which map layout we're using,
                       // second & third for obstacle coordinates
int gameState = 0;
int currentMap = 0;     // by default, use the first map layout
#define TITLE_SCREEN 0
#define IN_GAME 1
#define WINNER_A 2
#define WINNER_B 3

void eraseShips(ship *shipA, ship *shipB);
void eraseOldBullets( bullet *bulletArray);
void handleCollisions(bullet* bulletArray, int**** mapLayout, ship* shipA, ship* shipB, int currentMap);
void checkWinner(ship *shipA, ship* shipB);
ship *shipA; ship *shipB;   //ship pointers
bullet bulletArray[];   //array to keep track of all bullet objects

int main()
{

    while (gameState = TITLE_SCREEN)
    {
        // hi
    }

    while (gameState = IN_GAME)
    { // gameplay while loop
        // hi

        eraseShips(shipA, shipB); // erase previous spaceships
        eraseOldBullets(bulletArray); // erase previous bullets

        // handle bullet collisions (obstacles, or spaceship) + bullet deletion
        handleCollisions(bulletArray, mapLayout, shipA, shipB, currentMap);
        // update spaceship health

        checkWinner(shipA, shipB); // check for winner; if health = 0, clear screen, switch states
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