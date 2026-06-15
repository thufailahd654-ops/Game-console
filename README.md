# Game-console
A handheld retro game console built using an ESP32, a 0.96" SSD1306 OLED display, and a 4-button controller.
# RAWBOTICS Play Station

A handheld retro game console built using an ESP32, a 0.96" SSD1306 OLED display, and a 4-button controller.

This project contains multiple classic mini-games inside a single device, featuring a menu system, score saving with EEPROM, and custom game graphics.

## Features

* 6 Built-In Games
  * Flappy Bird
  * Brick Blast
  * Snake
  * Maze Escape
  * Fighter Jet
  * Plane Game

* OLED graphical interface
* Main menu navigation system
* EEPROM high score saving
* Animated boot screen
* Simple 4-button controls
* Lightweight and portable

---

# Hardware Used

## Main Components

* ESP32 Development Board
* SSD1306 0.96" OLED Display (128×64, I2C)
* 4 Push Buttons or RC Receiver Outputs
* Jumper Wires
* USB Cable
* Breadboard (optional)

---

# Pin Connections

## OLED Display

| OLED Pin | ESP32 Pin |
|----------|-----------|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA (Default ESP32 SDA) |
| SCL | SCL (Default ESP32 SCL) |

---

## Control Buttons

| Function | ESP32 Pin |
|-----------|-----------|
| UP | GPIO 18 |
| DOWN | GPIO 19 |
| LEFT | GPIO 25 |
| RIGHT | GPIO 26 |

---

# Control Layout

| Button | Action |
|----------|----------|
| UP | Move Up / Jump / Menu Up |
| DOWN | Move Down / Menu Down |
| LEFT | Return To Menu |
| RIGHT | Select / Start / Confirm |

---

# Wiring Diagram (Text)

ESP32

GPIO18 -------- UP Button -------- GND

GPIO19 -------- DOWN Button ------ GND

GPIO25 -------- LEFT Button ------ GND

GPIO26 -------- RIGHT Button ----- GND

3.3V ---------- OLED VCC

GND ----------- OLED GND

SDA ----------- OLED SDA

SCL ----------- OLED SCL

---

# How It Works

When powered on:

1. RAWBOTICS boot screen appears.
2. Main menu opens.
3. UP and DOWN buttons scroll through games.
4. RIGHT button launches the selected game.
5. LEFT button returns to the menu.
6. High scores are automatically saved in EEPROM.

---

# EEPROM Storage

The console stores high scores even after power is turned off.

| Game | EEPROM Address |
|--------|--------|
| Flappy Bird | 0 |
| Brick Blast | 4 |
| Snake | 8 |
| Maze Escape | 12 |
| Fighter Jet | 16 |
| Plane Game | 20 |

---

# Games Included

## Flappy Bird

Navigate through moving pipes.

### Controls
* UP = Flap
* LEFT = Menu

### Objective
Avoid hitting pipes and achieve the highest score possible.

---

## Brick Blast

Classic brick breaker game.

### Controls
* LEFT = Move Paddle Left
* RIGHT = Move Paddle Right

### Objective
Destroy all bricks and advance through levels.

---

## Snake

Classic snake game.

### Objective
Eat food, grow longer, and avoid crashing into yourself.

---

## Maze Escape

Navigate through a maze and reach the destination.

---

## Fighter Jet

Arcade-style flying game.

### Objective
Avoid obstacles and survive as long as possible.

---

## Plane Game

A variation of Flappy Bird where buildings replace pipes.

### Objective
Fly through gaps between buildings.

---

# Libraries Required

Install the following libraries using Arduino IDE Library Manager:

* Adafruit GFX Library
* Adafruit SSD1306
* EEPROM (included with ESP32 package)
* Wire (included)

---

# Installation

## Step 1

Install Arduino IDE.

## Step 2

Install ESP32 Board Package.

## Step 3

Install all required libraries.

## Step 4

Open `game_console.ino`.

## Step 5

Select:

Tools → Board → ESP32 Dev Module

## Step 6

Connect the ESP32 through USB.

## Step 7

Select the correct COM Port.

## Step 8

Upload the code.

---

# Project Structure

game_console.ino

Contains:

* Boot Screen
* Menu System
* Flappy Bird
* Brick Blast
* Snake
* Maze Escape
* Fighter Jet
* Plane Game
* EEPROM Score Saving

---

# Future Improvements

* Sound Effects
* Battery Power Support
* Rechargeable Charging Circuit
* More Games
* Multiplayer Mode
* Better Graphics
* Custom Startup Animations

---

# Creator

RAWBOTICS

"Learn robotics. Build along."

Created by Thufail Ahmed.
