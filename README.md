# Dropship, an ECE243 Project

## Project Summary

Dropship is a competitive top-down shooter implemented using Intel's NIOS II soft processor on the DE1-SoC board. It was
developed by Abhi Kupendiran and Zachary Hu as a final project for ECE243, a course on Computer Organization at the 
University of Toronto. Two players duel each other in spaceships in an asteroid field firing ricocheting bullets. As
time progresses, the playable area gradually shrinks as a damaging ring closes in. 

## How to Play

In order to play the game, you need two PS2 Keyboards, and a Y-Splitter in order to connect both players to the DE1-SoC
board. When first booting the game, a menu screen appears. Pressing “1” on either keyboard will begin the game. Each player
takes one keyboard, using “WASD” for movement, and the arrow keys to fire in a chosen direction. The game ends once one
player’s health reaches 0, in which the game over screen appears, showcasing who won the round. The game can be replayed
by pressing “1” on either keyboard.

The entirety of the project is contained in the main.c file, and can be compiled in Intel's Monitor Program, or
alternatively, a NIOS II simulator like Henry Wong's CPUlator (https://cpulator.01xz.net/?sys=nios-de1soc) 

> [!WARNING]
> Do note that trying to compile in simulators may cause issues with the size of the project; in this case, it may be
> necessary to initialize one of the storage heavy arrays (sample or sprites) as a zero array instead. While this means
> that specific sound / image won't be displayed in the game, you will still be able to see most of the project's functionality.

## Features

Shown below is a list of all of the features incorporated in our project, as well as who was responsible for each feat.

### Abhi
- PS2 Keyboard Interfacing
- Player Ship movement, including collision system
  - Used a collision mask, and would check pixel steps in the desired x and y direction for each frame, to ensure
    players would not clip past barriers or walls
- Player Shooting, including:
  - Handling bullet physics, allowing for ricocheting 
  - Implementing dynamic linked lists to keep track of the bullets
  - Adding an input buffer to help smoothen inputs
- Back-end Player systems
  - Controlling when players can fire
  - Damaging players when hit by bullets
- Ring System

### Zach
- Audio System
  - Interrupt based solution to sending out samples at arbitrary points in time, allowing for a background theme song as
    well as the dynamic playing of shooting sounds
  - Initialized C arrays for the background music as well as the shooting sound
- VGA work, including:
  - Incorporated double buffering in the drawing and erasing of objects like ships and bullets
  - Initialized C arrays for ship sprites, as well as touched up the images using pixel art editors to mitigate loss in
    quality from conversions
- Game State transitions
  - Established loops for each state of the game, such as waiting for user input in the title screen and winning screen
    states
  - Initialized C arrays for all background images, such as winning screens, title screen, and the level background
  - Managed the drawing of new backgrounds and erasing of old ones in transitioning from state to state
- Level Layouts
  - Drew obstacles in the background and updated Abhi’s collision frame array to align background obstacles with collision
    checks
