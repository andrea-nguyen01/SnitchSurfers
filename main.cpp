// Lab 10 - Snitch Surfers

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "PLL.h"
#include "ST7735.h"
#include "random.h"
#include "PLL.h"
//#include "SlidePot.h"
#include "Images.h"
#include "UART.h"
#include "Timer0.h"
#include "Timer1.h"
#include "Timer3.h"
#include "Sprite.h"
#include "ADC.h"
#include "Timer2.h"
#include "DAC.h"
//#include "Sound.h"

//SlidePot my(2000,0);

extern "C" void DisableInterrupts(void);
extern "C" void EnableInterrupts(void);
extern "C" void SysTick_Handler(void);

/* Globals */
bool gameOver;
uint32_t GAME_SCORE = 0;
// Instantiate the character in the middle lane
Sprite player(50, 155, charLeftLeg, 30, 35, 1);
Sprite Valvano(12, 0, ValvanoFace, 30, 31, 0);
Sprite Yerra(52, 0, YogaY, 30, 25, 2);
Sprite Gliggy(1 * 40 + 10, 0, GligoricFace, 30, 33, 1);
Sprite Pedro(1 * 40 + 10, 0, PedroFace, 30, 34, 1);
// Change coordinates and size
Sprite Cash(2 * 40 + 10, 0, Coin, 25, 30, 2);
Sprite Soap(1 * 40 + 10, 0, Shampoo, 28, 33, 1);

// Sprites on Screen
Sprite OBSTACLES[6] = {Valvano, Yerra, Gliggy, Pedro, Soap, Cash};
Sprite OBJ_ON_SCREEN[7] = {Valvano, Pedro, Cash, Gliggy, Soap, Gliggy, Yerra};
bool laneExists[3] = {false, false, false};

void GPIO_Init(){
  SYSCTL_RCGCGPIO_R |= 0x10; //clock on port E
  GPIO_PORTE_DIR_R &= 0xF0; //PE0-3 are inputs
  GPIO_PORTE_DEN_R |= 0xF;
}

// FUNCTION PROTOTYPES
void drawMainMenu();

// CONSTANT VARIABLES
// Lanes
const uint8_t LEFT_LANE = 0;
const uint8_t MIDDLE_LANE = 1;
const uint8_t RIGHT_LANE = 2;
// Hex Colors
const uint16_t ORANGE_SCREEN = ST7735_Color565(236, 125, 32); //0x23FD;
const uint16_t BLUE_SCREEN = ST7735_Color565(137, 220, 250);
// Animation Picture Arrays
const uint16_t *playerImages[2] = {charLeftLeg, charRightLeg};
// Language options
typedef enum {English, Spanish} Language_t;
// GLOBAL VARIABLES
Language_t CURRENT_LANGUAGE = English;


void moveCursorDown(int oldX, int oldY){
	ST7735_SetCursor(oldX,oldY); 
  ST7735_OutString("   ");    // erase old cursor

  ST7735_SetCursor(oldX,(oldY+1));
  ST7735_OutString(">>> ");        // write new cursor 
}

void moveCursorUp(int oldX, int oldY){

	ST7735_SetCursor(oldX,oldY); 
  ST7735_OutString("   ");    // erase old cursor

  ST7735_SetCursor(oldX,(oldY-1));
  ST7735_OutString(">>> ");        // write new cursor 

}

uint8_t getMenuChoice() {
	uint8_t MenuChoice = 1; 
	while(!(GPIO_PORTE_DATA_R & 0x01)){ //wait for select button to be pressed
      if(joystickY() > 3650 && MenuChoice != 0){
       
				if(MenuChoice == 1){
					moveCursorUp(2, 8);
					MenuChoice = 0;
          while(joystickY() > 3650){};
          continue;
        
        }
        if(MenuChoice == 2){
					moveCursorUp(2, 9);
          MenuChoice = 1;
          while(joystickY() > 3650){};
          continue;
        }
			}
			
			
      if(joystickY() < 150 && MenuChoice != 2){
        if(MenuChoice == 0){
					moveCursorDown(2, 7);
          MenuChoice = 1;
          while(joystickY() < 150){};
          continue;
        }
        if(MenuChoice == 1){
					moveCursorDown(2, 8);
          MenuChoice = 2;
          while(joystickY() < 150){};
					continue;
        }
      }
    }
	while((GPIO_PORTE_DATA_R & 0x01)){}
	return MenuChoice;
}

void drawMainMenu(){
	ST7735_FillScreen(0x0000); 
	ST7735_DrawBitmap(15, 51, GameTitle, 100, 50);
	
	if(CURRENT_LANGUAGE == English){
		ST7735_SetCursor(6,7);
    ST7735_OutString("Play ");
    ST7735_SetCursor(6,8);
    ST7735_OutString("English");
    ST7735_SetCursor(6,9);
    ST7735_OutString("Instructions ");
	}
	if(CURRENT_LANGUAGE == Spanish){
		ST7735_SetCursor(6,7);
    ST7735_OutString("Jugar");
    ST7735_SetCursor(6,8);
    ST7735_OutString("Espa\xA4ol\0");
    ST7735_SetCursor(6,9);
    ST7735_OutString("Instrucciones");
	}
	
		ST7735_SetCursor(2, 8);
    ST7735_OutString(">>> ");			//set default cursor to language 
}

void switchLanguage() {
	if(CURRENT_LANGUAGE == English){
		CURRENT_LANGUAGE = Spanish;
	} else if(CURRENT_LANGUAGE == Spanish){
		CURRENT_LANGUAGE = English;
	}
	drawMainMenu();
}


// FUNCTIONS FOR TIMERS
uint8_t legSide = 0;
uint8_t PLAYER_FLAG = 0;
void UPDATE_PLAYER_POS(void){
	player.setSprite(playerImages[legSide]);
	legSide = !legSide;
	PLAYER_FLAG = 1;
}
uint8_t OBSTACLE_FLAG = 0;
void UPDATE_OBSTACLE_POS(void){
	static int delay = 0;
	if(delay == 100){
		GAME_SCORE++;
		delay = 0;
	}
	delay++;
	OBSTACLE_FLAG = 1;
}

// IMPLEMENTATION FOR gameEngine + corresponding helper functions
// Helper Prototypes
void gameInit();
void checkButtonInput();
void drawScore();
void animatePlayer();
void getPlayerInput();
void generateObstacles();
void animateObstacles();
void checkObstacleCollisions();
// Functionality: Engine that controls and runs the game
void gameEngine(){
	gameInit();
	while(!gameOver){
		checkButtonInput();
		drawScore();
		animateObstacles();
		animatePlayer();
		getPlayerInput();
		checkObstacleCollisions();
	}
}

void checkButtonInput(){
	static bool SW3_HELD = false;
	// Checks if SW3 was released after it was pressed
	if(!(GPIO_PORTE_DATA_R & 0x04) && SW3_HELD){
		SW3_HELD = false;
		// Pause the game when the button isn't being pressed
		while(!(GPIO_PORTE_DATA_R & 0x04)){}
		// Wait for button to be released to resume the game	
		while((GPIO_PORTE_DATA_R & 0x04)){}
	}
	// Check if SW3 was pressed
	if((GPIO_PORTE_DATA_R & 0x04) && !SW3_HELD){
		SW3_HELD = true;
	}

// Restart the game if SW1 is pressed and released	
	static bool SW1_HELD = false;
	// Checks if SW4 was released after it was pressed
	if(!(GPIO_PORTE_DATA_R & 0x01) && SW1_HELD){
		SW1_HELD = false;
		gameEngine();
	}
	// Check if SW4 was pressed
	if((GPIO_PORTE_DATA_R & 0x01) && !SW1_HELD){
		SW1_HELD = true;
	}
}
void drawScore(){
	ST7735_SetCursor(0, 0);
	if(CURRENT_LANGUAGE == English){
		ST7735_OutString("Score: ");
		ST7735_SetCursor(6, 0);
		ST7735_OutUDec(GAME_SCORE);
	}else if(CURRENT_LANGUAGE == Spanish){
		ST7735_OutString("Puntos: ");	
		ST7735_SetCursor(8, 0);
		ST7735_OutUDec(GAME_SCORE);
	}
}
// Helpers for gameInit()
void drawGame();
// Functionality: Initializes timers and game sounds
void gameInit(){
	DisableInterrupts();
	// Timers to animate our sprites
	gameOver = false;
	GAME_SCORE = 0;
	Timer3_Init(&UPDATE_PLAYER_POS, 10000000); 
	Timer1_Init(&UPDATE_OBSTACLE_POS, 1500000); 
	drawGame();
	generateObstacles();
	TIMER0_CTL_R = 0x00000001;
	EnableInterrupts();
}
// Functionality: Draw the game map & player 
void drawGame(){
	ST7735_FillScreen(ORANGE_SCREEN); 
	ST7735_DrawBitmap(0, 160, gameBackground, 128, 160);
}

// Functionality: Updates the player's position and running animation
void animatePlayer(){
	if(PLAYER_FLAG){
		player.drawSprite();
		PLAYER_FLAG = 0;
	}
}
bool JOYSTICK_HELD = false;
// Functionality: Sets the player's position based on slide and release of joystick input
void getPlayerInput(){
	if(joystickX() < 3650 && joystickX() > 350){
		if(JOYSTICK_HELD){
			// Sound_Air();
		}
		JOYSTICK_HELD = false;
	}
	if(joystickX() > 3650 && player.getLane() != 0 && !JOYSTICK_HELD){
		player.eraseSprite();
		player.updatePos(player.getX() - 40, player.getLane() - 1); 
		JOYSTICK_HELD = true;
	}
	if(joystickX() < 350 && player.getLane() != 2 && !JOYSTICK_HELD){
		player.eraseSprite();
		player.updatePos(player.getX() + 40, player.getLane() + 1); 
		JOYSTICK_HELD = true;
	}	
}
int initY = 0;
// Functionality: Generates 3 obstacles onto the screen
void generateObstacles(){
	initY = 0;
	for(int i = 0; i < 7; i++){
		int obstacleType = Random() % 4;
		OBJ_ON_SCREEN[i] = OBSTACLES[obstacleType];	
		OBJ_ON_SCREEN[i].setLane(Random() % 3);
		OBJ_ON_SCREEN[i].setX(OBJ_ON_SCREEN[i].getLane() * 40 + 10);
		OBJ_ON_SCREEN[i].setY(initY);
		initY -= 60;
	}
}

// Functionality: Generates a new obstacles for every obstacle that goes off screen
void createNewObstacle(uint8_t obstacleIndex){
	//static int spawnCount = 0;
	OBJ_ON_SCREEN[obstacleIndex] = OBSTACLES[Random() % 6];
	OBJ_ON_SCREEN[obstacleIndex].setLane(Random() % 3);
	OBJ_ON_SCREEN[obstacleIndex].setX(OBJ_ON_SCREEN[obstacleIndex].getLane() * 40 + 10);
	initY -= 60;
	OBJ_ON_SCREEN[obstacleIndex].setY(initY);
}

// Functionality: Animates the obstacles moving towards the player
void animateObstacles(){
	int dy = 1;
	if(OBSTACLE_FLAG){
		for(int i = 0; i < 7; i++){
			OBJ_ON_SCREEN[i].setY(dy);
			OBJ_ON_SCREEN[i].drawSprite();
		}			
		initY++;
		OBSTACLE_FLAG = 0;
	}
}

// Helpers:
void displayGameOver();
void createNewObstacle(uint8_t obstacleIndex);
//Functionality: Checks for collisions and generates new obstacles accordingly
void checkObstacleCollisions(){
	// This loop runs once for every obstacle
	for(int i = 0; i < 7; i++){
		// Check collision w/ obstacle
		if(OBJ_ON_SCREEN[i].getY() > 140 && OBJ_ON_SCREEN[i].getY() < (140 + 30) && (player.getLane() == OBJ_ON_SCREEN[i].getLane())){
			const uint16_t * collidedSprite = OBJ_ON_SCREEN[i].getSprite();
			if(collidedSprite == ValvanoFace || collidedSprite == YogaY || collidedSprite == GligoricFace || collidedSprite == PedroFace){
				gameOver = true;
				displayGameOver();
				break;
			} 
			if(collidedSprite == Coin || collidedSprite == Shampoo){
				OBJ_ON_SCREEN[i].eraseSprite();
				GAME_SCORE += 10;
				createNewObstacle(i);
			}
		}
		// Generate new obstacle if it's off screen
		if(OBJ_ON_SCREEN[i].getY() >= 200){
			createNewObstacle(i);
		}
	}
}
void displayGameOver(){
    DisableInterrupts();
    ST7735_FillScreen(ST7735_BLACK);
		if(CURRENT_LANGUAGE == English){
			ST7735_SetCursor(6,6);
			ST7735_OutString("GAME OVER");
			ST7735_SetCursor(6,7);
			ST7735_OutString("SCORE: ");
			ST7735_SetCursor(13,7);
			ST7735_OutUDec(GAME_SCORE);
		}else if(CURRENT_LANGUAGE == Spanish){
			ST7735_SetCursor(4,6);
			ST7735_OutString("FIN DEL JUEGO");
			ST7735_SetCursor(5,7);
			ST7735_OutString("PUNTOS: ");
			ST7735_SetCursor(13,7);
			ST7735_OutUDec(GAME_SCORE);
		}
    while(!(GPIO_PORTE_DATA_R & 0x01)){}
    while(GPIO_PORTE_DATA_R & 0x01){}        //wait for bottom button press and release to return
		drawMainMenu();

}

void getInstructions(void) {
  ST7735_FillScreen(ORANGE_SCREEN);
	if(CURRENT_LANGUAGE == English){
		ST7735_SetCursor(0,0);
		ST7735_OutString("Instructions:");
		ST7735_SetCursor(3, 3);
		ST7735_OutString("Avoid Obstacles");
		ST7735_DrawBitmap(5, 71, ValvanoFace, 30, 31);
		ST7735_DrawBitmap(36, 71, YogaY, 30, 25);
		ST7735_DrawBitmap(66, 71, GligoricFace, 30, 33);
		ST7735_DrawBitmap(96, 71, PedroFace, 30, 34);
		ST7735_SetCursor(5, 9);
		ST7735_OutString("+10 Points");
		ST7735_DrawBitmap(34, 131, Coin, 25, 30);
		ST7735_DrawBitmap(64, 131, Shampoo, 28, 33);
	}
	if(CURRENT_LANGUAGE == Spanish){
		ST7735_SetCursor(0,0);
		ST7735_OutString("Instrucciones:");
		ST7735_SetCursor(1, 3);
		ST7735_OutString("Esquivar Obst\xA0""culos");
		ST7735_DrawBitmap(5, 71, ValvanoFace, 30, 31);
		ST7735_DrawBitmap(36, 71, YogaY, 30, 25);
		ST7735_DrawBitmap(66, 71, GligoricFace, 30, 33);
		ST7735_DrawBitmap(96, 71, PedroFace, 30, 34);
		ST7735_SetCursor(5, 9);
		ST7735_OutString("+10 Puntos");
		ST7735_DrawBitmap(34, 131, Coin, 25, 30);
		ST7735_DrawBitmap(64, 131, Shampoo, 28, 33);
	}
	while(!(GPIO_PORTE_DATA_R & 0x01)){}
  drawMainMenu();
}

// OUR MAIN() :) 
int main(void){
  DisableInterrupts();
	PLL_Init(Bus80MHz);       // Bus clock is 80 MHz 
  // TExaS_Init();
  Random_Init(1);
  Output_Init();
	ADC_Init();
	GPIO_Init();
	// backgroundMusicInit();
	// Sound_Init();
	EnableInterrupts();
	
	// Load the main menu
	ST7735_InvertDisplay(100);  
	ST7735_SetRotation(2);
	drawMainMenu();
	while(1){
		int menuChoice = getMenuChoice();
		if(menuChoice == 0){
			gameEngine();
		}
		if(menuChoice == 1){
			switchLanguage();
		}
		if(menuChoice == 2){
			getInstructions();
		}
	}

}