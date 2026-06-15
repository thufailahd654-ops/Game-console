#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_W 128
#define SCREEN_H  64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ── Buttons ────────────────────────────────────────────────────────────
#define PIN_F 18  // UP
#define PIN_B 19  // DOWN
#define PIN_L 25  // LEFT  (back to menu)
#define PIN_R 26  // RIGHT (select)

// ── EEPROM layout (2 bytes per score, 6 games) ─────────────────────────
#define EEPROM_SIZE 32
#define HS_FLAPPY   0
#define HS_BRICK    4
#define HS_SNAKE    8
#define HS_MAZE    12
#define HS_JET     16
#define HS_PLANE   20

// ── Game IDs ───────────────────────────────────────────────────────────
#define GAME_NONE    -1
#define GAME_FLAPPY   0
#define GAME_BRICK    1
#define GAME_SNAKE    2
#define GAME_MAZE     3
#define GAME_JET      4
#define GAME_PLANE    5
#define NUM_GAMES     6

int currentGame = GAME_NONE;
int menuIndex   = 0;

const char* gameNames[NUM_GAMES] = {
  "Flappy Bird",
  "Brick Blast",
  "Snake",
  "Maze Escape",
  "Fighter Jet",
  "Plane Game"
};

// ── Button state ───────────────────────────────────────────────────────
bool prevF, prevB, prevL, prevR;
bool btnF, btnB, btnL, btnR;
bool pressF, pressB, pressL, pressR;

void readButtons() {
  auto rd = [](int pin) -> bool {
  if (digitalRead(pin) != LOW) return false;
  delay(10);
  return digitalRead(pin) == LOW;
};
  btnF = rd(PIN_F); btnB = rd(PIN_B);
  btnL = rd(PIN_L); btnR = rd(PIN_R);
  pressF = btnF && !prevF;
  pressB = btnB && !prevB;
  pressL = btnL && !prevL;
  pressR = btnR && !prevR;
  prevF = btnF; prevB = btnB;
  prevL = btnL; prevR = btnR;
}

// ── EEPROM helpers ─────────────────────────────────────────────────────
int  loadHS(int addr)       { int v=0; EEPROM.get(addr,v); return (v<0||v>99999)?0:v; }
void saveHS(int addr, int v){ EEPROM.put(addr,v); EEPROM.commit(); }

// ══════════════════════════════════════════════════════════════════════
//  BOOT SCREEN
// ══════════════════════════════════════════════════════════════════════
void showBoot() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Big title
  display.setTextSize(2);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds("RAWBOTICS", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, 8);
  display.print("RAWBOTICS");

  display.setTextSize(1);
  display.getTextBounds("PLAY STATION", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, 32);
  display.print("PLAY STATION");

  display.drawFastHLine(10, 44, SCREEN_W-20, WHITE);
  display.setCursor(28, 50);
  display.print("6 GAMES INSIDE");

  display.display();
  delay(2500);
}

// ══════════════════════════════════════════════════════════════════════
//  MAIN MENU
// ══════════════════════════════════════════════════════════════════════
void runMenu() {
  currentGame = GAME_NONE;
  unsigned long enter = millis();

  while (true) {
    readButtons();

    if (pressB) menuIndex = (menuIndex + 1) % NUM_GAMES;
    if (pressF) menuIndex = (menuIndex - 1 + NUM_GAMES) % NUM_GAMES;
    if (pressR) { currentGame = menuIndex; return; }

    display.clearDisplay();
    display.setTextColor(WHITE);

    // Header
    display.setTextSize(1);
    display.setCursor(22, 0);
    display.print("RAWBOTICS  PS");
    display.drawFastHLine(0, 9, SCREEN_W, WHITE);

    // Show 3 games centred around selection
    for (int i = -1; i <= 1; i++) {
      int idx = (menuIndex + i + NUM_GAMES) % NUM_GAMES;
      int y   = 22 + i * 14;
      if (i == 0) {
        display.fillRoundRect(0, y - 2, SCREEN_W, 13, 2, WHITE);
        display.setTextColor(BLACK);
        display.setCursor(4, y);
        display.print(">");
        display.setCursor(14, y);
        display.print(gameNames[idx]);
        display.setTextColor(WHITE);
      } else {
        display.setCursor(14, y);
        display.print(gameNames[idx]);
      }
    }

    display.drawFastHLine(0, 54, SCREEN_W, WHITE);
    display.setCursor(4, 57);
    display.print("UP/DN=scroll  R=play");

    display.display();
    delay(80);
  }
}
// ══════════════════════════════════════════════════════════════════════
//  GAME 1 — FLAPPY BIRD  &  GAME 6 — PLANE GAME  (shared engine)
// ══════════════════════════════════════════════════════════════════════
#define GRAVITY      0.38f
#define FLAP_VEL    -3.2f
#define BASE_SPEED   1.4f
#define SPEED_INC    0.0012f
#define PIPE_GAP     32
#define PIPE_WIDTH   10
#define PIPE_SPACING 52
#define BIRD_X       22
#define BIRD_W       10
#define BIRD_H        8

struct Pipe { float x; int gapY; };
void resetPipe(Pipe &p, float sx);
void drawPipe(Pipe &p, bool isPlane);
bool hitPipe(Pipe &p);

float fb_birdY, fb_birdVel, fb_pipeSpeed;
int   fb_score;
bool  fb_gameOver, fb_started;
unsigned long fb_lastFrame;
Pipe  fb_pipes[2];
int   fb_wingFrame, fb_wingTick;
bool  fb_flapTriggered;

int fb_randomGap() { return random(6, SCREEN_H - PIPE_GAP - 6); }

void resetPipe(Pipe &p, float sx) { p.x = sx; p.gapY = fb_randomGap(); }

void drawBirdSprite(int x, int y) {
  display.drawRoundRect(x, y+1, BIRD_W, BIRD_H-2, 2, WHITE);
  display.fillRoundRect(x+1, y+2, BIRD_W-2, BIRD_H-4, 1, WHITE);
  display.drawPixel(x+BIRD_W-3, y+2, BLACK);
  display.drawPixel(x+BIRD_W,   y+3, WHITE);
  display.drawPixel(x+BIRD_W,   y+4, WHITE);
  display.drawPixel(x+BIRD_W+1, y+3, WHITE);
  display.drawPixel(x-1, y+2, WHITE);
  display.drawPixel(x-1, y+5, WHITE);
  if (fb_wingFrame==0) display.drawFastHLine(x+2,y+BIRD_H/2,BIRD_W-4,BLACK);
  else if (fb_wingFrame==1){
    display.drawPixel(x+2,y,WHITE); display.drawPixel(x+3,y-1,WHITE);
    display.drawPixel(x+4,y-1,WHITE); display.drawPixel(x+5,y,WHITE);
    display.drawFastHLine(x+2,y+1,4,BLACK);
  } else {
    display.drawPixel(x+2,y+BIRD_H,WHITE); display.drawPixel(x+3,y+BIRD_H+1,WHITE);
    display.drawPixel(x+4,y+BIRD_H+1,WHITE); display.drawPixel(x+5,y+BIRD_H,WHITE);
    display.drawFastHLine(x+2,y+BIRD_H-2,4,BLACK);
  }
}

void drawPlaneSprite(int x, int y) {
  // Fuselage
  display.fillRect(x, y+2, BIRD_W-1, 4, WHITE);
  // Nose
  display.drawPixel(x+BIRD_W,   y+3, WHITE);
  display.drawPixel(x+BIRD_W+1, y+3, WHITE);
  display.drawPixel(x+BIRD_W,   y+4, WHITE);
  // Top wing
  display.drawFastHLine(x+2, y,   5, WHITE);
  // Bottom wing
  display.drawFastHLine(x+3, y+7, 4, WHITE);
  // Tail
  display.drawPixel(x-1, y+1, WHITE);
  display.drawPixel(x-1, y+2, WHITE);
  // Engine detail
  display.drawPixel(x+4, y+3, BLACK);
}

void drawPipe(Pipe &p, bool isPlane) {
  int px = (int)p.x;
  if (isPlane) {
    // Buildings instead of pipes
    // Top building
    display.fillRect(px, 0, PIPE_WIDTH, p.gapY, WHITE);
    // windows
    for (int wy = 2; wy < p.gapY - 2; wy += 5)
      display.drawPixel(px+2, wy, BLACK);
    // rooftop ledge
    display.fillRect(px-2, p.gapY-4, PIPE_WIDTH+4, 4, WHITE);
    // Bottom building
    int botTop = p.gapY + PIPE_GAP;
    display.fillRect(px, botTop, PIPE_WIDTH, SCREEN_H-botTop, WHITE);
    for (int wy = botTop+2; wy < SCREEN_H-2; wy += 5)
      display.drawPixel(px+2, wy, BLACK);
    display.fillRect(px-2, botTop, PIPE_WIDTH+4, 4, WHITE);
  } else {
    display.fillRect(px, 0, PIPE_WIDTH, p.gapY, WHITE);
    display.fillRect(px-1, p.gapY-3, PIPE_WIDTH+2, 3, WHITE);
    int botTop = p.gapY + PIPE_GAP;
    display.fillRect(px, botTop, PIPE_WIDTH, SCREEN_H-botTop, WHITE);
    display.fillRect(px-1, botTop, PIPE_WIDTH+2, 3, WHITE);
  }
}

bool hitPipe(Pipe &p) {
  int bx1=BIRD_X, bx2=BIRD_X+BIRD_W;
  int by1=(int)fb_birdY, by2=(int)fb_birdY+BIRD_H;
  int px1=(int)p.x-1, px2=(int)p.x+PIPE_WIDTH+1;
  if (bx2<px1||bx1>px2) return false;
  if (by1<p.gapY||by2>p.gapY+PIPE_GAP) return true;
  return false;
}

void fb_init() {
  fb_birdY=SCREEN_H/2.0f-BIRD_H/2.0f; fb_birdVel=0;
  fb_pipeSpeed=BASE_SPEED; fb_score=0;
  fb_gameOver=false; fb_started=false;
  fb_wingFrame=0; fb_wingTick=0; fb_flapTriggered=false;
  resetPipe(fb_pipes[0], SCREEN_W+10);
  resetPipe(fb_pipes[1], SCREEN_W+10+PIPE_SPACING);
}

void runFlappyEngine(bool isPlane) {
  int hsAddr = isPlane ? HS_PLANE : HS_FLAPPY;
  int hiScore = loadHS(hsAddr);
  fb_init();
  fb_lastFrame = millis();

  while (true) {
    if (millis()-fb_lastFrame < 33) continue;
    fb_lastFrame = millis();
    readButtons();

    // Left = back to menu
    if (pressL) return;

    bool fPress = pressF;

    if (fb_gameOver) {
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(34,2); display.print("GAME OVER");
      display.drawFastHLine(0,13,SCREEN_W,WHITE);
      display.setCursor(10,18); display.print("Score : "); display.print(fb_score);
      display.setCursor(10,29); display.print("Best  : "); display.print(hiScore);
      if (fb_score>=hiScore && fb_score>0){
        display.setCursor(20,40); display.print("NEW RECORD!");
      }
      display.drawFastHLine(0,50,SCREEN_W,WHITE);
      display.setCursor(2,55); display.print("R=restart  L=menu");
      display.display();
      if (pressR) { fb_init(); hiScore=loadHS(hsAddr); }
      if (pressL) return;
      continue;
    }

    if (!fb_started) {
      fb_wingTick++;
      if (fb_wingTick>8){ fb_wingTick=0; fb_wingFrame=(fb_wingFrame+1)%3; }
      display.clearDisplay();
      display.drawFastHLine(0,SCREEN_H-4,SCREEN_W,WHITE);
      float bob=sin(millis()/300.0f)*2.0f;
      if (isPlane) drawPlaneSprite(BIRD_X,(int)(SCREEN_H/2-BIRD_H/2+bob));
      else         drawBirdSprite(BIRD_X,(int)(SCREEN_H/2-BIRD_H/2+bob));
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(isPlane?44:52, 10);
      display.print(isPlane?"PLANE GAME":"FLAPPY BIRD");
      display.setCursor(28,30); display.print("UP to start");
      display.setCursor(22,42); display.print("L=menu");
      if (hiScore>0){ display.setCursor(32,54); display.print("Best:"); display.print(hiScore); }
      display.display();
      if (fPress){ fb_started=true; fb_birdVel=FLAP_VEL; fb_wingFrame=1; fb_wingTick=0; }
      continue;
    }

    // Playing
    if (fPress){ fb_birdVel=FLAP_VEL; fb_wingFrame=1; fb_wingTick=0; fb_flapTriggered=true; }
    int ws=fb_flapTriggered?4:9;
    fb_wingTick++;
    if (fb_wingTick>=ws){
      fb_wingTick=0;
      if (fb_wingFrame==1) fb_wingFrame=0;
      else if (fb_wingFrame==0) fb_wingFrame=2;
      else { fb_wingFrame=0; fb_flapTriggered=false; }
    }
    fb_birdVel+=GRAVITY; fb_birdY+=fb_birdVel; fb_pipeSpeed+=SPEED_INC;
    if (fb_birdY<0){fb_birdY=0;fb_birdVel=0;}
    if (fb_birdY+BIRD_H>=SCREEN_H-3){
      fb_gameOver=true;
      if (fb_score>hiScore){hiScore=fb_score;saveHS(hsAddr,hiScore);}
      continue;
    }
    for (int i=0;i<2;i++){
      fb_pipes[i].x-=fb_pipeSpeed;
      if ((int)fb_pipes[i].x+PIPE_WIDTH==BIRD_X) fb_score++;
      if (fb_pipes[i].x+PIPE_WIDTH<0){
        float ox=fb_pipes[1-i].x;
        resetPipe(fb_pipes[i],ox+PIPE_SPACING);
      }
      if (hitPipe(fb_pipes[i])){
        fb_gameOver=true;
        if (fb_score>hiScore){hiScore=fb_score;saveHS(hsAddr,hiScore);}
      }
    }

    display.clearDisplay();
    display.drawFastHLine(0,SCREEN_H-4,SCREEN_W,WHITE);
    for (int gx=(int)(millis()/50)%16;gx<SCREEN_W;gx+=16)
      display.drawFastHLine(gx,SCREEN_H-2,6,WHITE);
    drawPipe(fb_pipes[0],isPlane);
    drawPipe(fb_pipes[1],isPlane);
    int bdy=(int)fb_birdY+(fb_birdVel>2.5f?1:0);
    if (isPlane) drawPlaneSprite(BIRD_X,bdy);
    else         drawBirdSprite(BIRD_X,bdy);
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(2,2); display.print(fb_score);
    display.display();
  }
}
// ══════════════════════════════════════════════════════════════════════
//  GAME 2 — BRICK BLAST
// ══════════════════════════════════════════════════════════════════════
#define BB_COLS      10
#define BB_ROWS       4
#define BB_BW        11   // brick width
#define BB_BH         5   // brick height
#define BB_PAD_W     20
#define BB_PAD_Y     (SCREEN_H-6)
#define BB_PAD_SPD    3
#define BB_BALL_R     2

struct BBrick { bool alive; int hp; };
BBrick bb_bricks[BB_ROWS][BB_COLS];
float  bb_padX;
float  bb_bx, bb_by, bb_vx, bb_vy;
int    bb_score, bb_lives;
bool   bb_gameOver, bb_started;
bool   bb_multiball;
int    bb_mbTimer;
// Multi-ball (up to 3 extra)
float  bb_ex[3], bb_ey[3], bb_evx[3], bb_evy[3];
bool   bb_eActive[3];

void bb_initBricks(int level) {
  for (int r=0;r<BB_ROWS;r++)
    for (int c=0;c<BB_COLS;c++){
      bb_bricks[r][c].alive = true;
      bb_bricks[r][c].hp    = (level>3)? 2 : 1;
    }
}

void bb_init(int level=1) {
  bb_padX   = (SCREEN_W-BB_PAD_W)/2.0f;
  bb_bx     = SCREEN_W/2.0f;
  bb_by     = BB_PAD_Y - BB_BALL_R - 2;
  float spd = 1.8f + level*0.15f;
  bb_vx     = spd; bb_vy = -spd;
  bb_lives  = 3;
  bb_score  = 0;
  bb_gameOver = false; bb_started = false;
  bb_multiball = false; bb_mbTimer = 0;
  for (int i=0;i<3;i++) bb_eActive[i]=false;
  bb_initBricks(level);
}

bool bb_allClear() {
  for (int r=0;r<BB_ROWS;r++)
    for (int c=0;c<BB_COLS;c++)
      if (bb_bricks[r][c].alive) return false;
  return true;
}

void bb_bounceBall(float &bx, float &by, float &vx, float &vy) {
  // Walls
  if (bx<=BB_BALL_R)         { bx=BB_BALL_R+1;      vx=fabs(vx); }
  if (bx>=SCREEN_W-BB_BALL_R){ bx=SCREEN_W-BB_BALL_R-1; vx=-fabs(vx); }
  if (by<=BB_BALL_R)         { by=BB_BALL_R+1;       vy=fabs(vy); }

  // Paddle
  if (by+BB_BALL_R>=BB_PAD_Y &&
      bx>=bb_padX-1 && bx<=bb_padX+BB_PAD_W+1) {
    by = BB_PAD_Y-BB_BALL_R-1;
    vy = -fabs(vy);
    // angle from centre of paddle
    float off = (bx-(bb_padX+BB_PAD_W/2.0f))/(BB_PAD_W/2.0f);
    vx = off * 2.5f;
    if (fabs(vx)<0.4f) vx=(vx>=0)?0.4f:-0.4f;
  }

  // Bricks
  for (int r=0;r<BB_ROWS;r++) {
    for (int c=0;c<BB_COLS;c++) {
      if (!bb_bricks[r][c].alive) continue;
      int bkX = c*(BB_BW+1)+1;
      int bkY = r*(BB_BH+1)+10;
      if (bx+BB_BALL_R>bkX && bx-BB_BALL_R<bkX+BB_BW &&
          by+BB_BALL_R>bkY && by-BB_BALL_R<bkY+BB_BH) {
        bb_bricks[r][c].hp--;
        if (bb_bricks[r][c].hp<=0) {
          bb_bricks[r][c].alive=false;
          bb_score+=10;
          // 1 in 8 chance: spawn multiball
          if (!bb_multiball && random(8)==0) {
            bb_multiball=true; bb_mbTimer=300;
            for (int i=0;i<3;i++){
              bb_eActive[i]=true;
              bb_ex[i]=bx; bb_ey[i]=by;
              bb_evx[i]=vx*(0.8f+i*0.2f)*(i%2?1:-1);
              bb_evy[i]=-fabs(vy);
            }
          }
        }
        // Decide bounce direction
        float overlapL=bx+BB_BALL_R-bkX;
        float overlapR=bkX+BB_BW-(bx-BB_BALL_R);
        float overlapT=by+BB_BALL_R-bkY;
        float overlapB=bkY+BB_BH-(by-BB_BALL_R);
        float minH=min(overlapL,overlapR);
        float minV=min(overlapT,overlapB);
        if (minH<minV) vx=-vx; else vy=-vy;
        return;
      }
    }
  }
}

void runBrickBlast() {
  int hiScore = loadHS(HS_BRICK);
  int level   = 1;
  bb_init(level);
  unsigned long lastF = millis();

  while (true) {
    if (millis()-lastF<33) continue;
    lastF=millis();
    readButtons();

    if (pressL && bb_gameOver) return;  // back to menu
    if (pressL && !bb_started) return;

    if (bb_gameOver) {
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(28,2);  display.print("BRICK BLAST");
      display.drawFastHLine(0,12,SCREEN_W,WHITE);
      display.setCursor(10,16); display.print("Score : "); display.print(bb_score);
      display.setCursor(10,26); display.print("Best  : "); display.print(hiScore);
      display.setCursor(10,36); display.print("Level : "); display.print(level);
      display.drawFastHLine(0,50,SCREEN_W,WHITE);
      display.setCursor(2,54);  display.print("R=restart  L=menu");
      display.display();
      if (pressR){ level=1; bb_init(level); hiScore=loadHS(HS_BRICK); }
      continue;
    }

    if (!bb_started) {
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(24,10); display.print("BRICK BLAST");
      display.setCursor(10,26); display.print("L/R move paddle");
      display.setCursor(10,36); display.print("Don't drop ball!");
      display.setCursor(10,50); display.print("R=start  L=menu");
      display.display();
      if (pressR) bb_started=true;
      continue;
    }

    // Move paddle
    if (btnL) bb_padX-=BB_PAD_SPD;
    if (btnR) bb_padX+=BB_PAD_SPD;
    if (bb_padX<0) bb_padX=0;
    if (bb_padX>SCREEN_W-BB_PAD_W) bb_padX=SCREEN_W-BB_PAD_W;

    // Move & bounce main ball
    bb_bx+=bb_vx; bb_by+=bb_vy;
    bb_bounceBall(bb_bx,bb_by,bb_vx,bb_vy);

    // Extra balls
    if (bb_multiball) {
      bb_mbTimer--;
      if (bb_mbTimer<=0) {
        bb_multiball=false;
        for (int i=0;i<3;i++) bb_eActive[i]=false;
      }
      for (int i=0;i<3;i++){
        if (!bb_eActive[i]) continue;
        bb_ex[i]+=bb_evx[i]; bb_ey[i]+=bb_evy[i];
        bb_bounceBall(bb_ex[i],bb_ey[i],bb_evx[i],bb_evy[i]);
        if (bb_ey[i]>SCREEN_H) bb_eActive[i]=false;
      }
    }

    // Ball fell
    if (bb_by>SCREEN_H) {
      bb_lives--;
      if (bb_lives<=0){
        bb_gameOver=true;
        if (bb_score>hiScore){ hiScore=bb_score; saveHS(HS_BRICK,hiScore); }
      } else {
        bb_bx=bb_padX+BB_PAD_W/2.0f; bb_by=BB_PAD_Y-BB_BALL_R-2;
        bb_vx=1.8f+level*0.15f; bb_vy=-(1.8f+level*0.15f);
        for (int i=0;i<3;i++) bb_eActive[i]=false;
        bb_multiball=false;
      }
    }

    // Level clear
    if (bb_allClear()){
      level++;
      bb_initBricks(level);
      bb_bx=bb_padX+BB_PAD_W/2.0f; bb_by=BB_PAD_Y-BB_BALL_R-2;
      float spd=1.8f+level*0.15f;
      bb_vx=spd; bb_vy=-spd;
      bb_score+=50;
    }

    // Draw
    display.clearDisplay();
    // Bricks
    for (int r=0;r<BB_ROWS;r++)
      for (int c=0;c<BB_COLS;c++){
        if (!bb_bricks[r][c].alive) continue;
        int bkX=c*(BB_BW+1)+1, bkY=r*(BB_BH+1)+10;
        if (bb_bricks[r][c].hp>1)
          display.drawRect(bkX,bkY,BB_BW,BB_BH,WHITE);
        else
          display.fillRect(bkX,bkY,BB_BW,BB_BH,WHITE);
      }
    // Paddle
    display.fillRect((int)bb_padX,BB_PAD_Y,BB_PAD_W,3,WHITE);
    // Ball
    display.fillCircle((int)bb_bx,(int)bb_by,BB_BALL_R,WHITE);
    // Extra balls
    for (int i=0;i<3;i++)
      if (bb_eActive[i])
        display.fillCircle((int)bb_ex[i],(int)bb_ey[i],BB_BALL_R,WHITE);
    // HUD
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0,0);   display.print(bb_score);
    display.setCursor(50,0);  display.print("Lv"); display.print(level);
    // Lives as hearts
    for (int i=0;i<bb_lives;i++){
      int hx=SCREEN_W-6-i*8;
      display.drawPixel(hx,1,WHITE); display.drawPixel(hx+2,1,WHITE);
      display.drawPixel(hx-1,2,WHITE); display.drawPixel(hx+3,2,WHITE);
      display.drawLine(hx-1,3,hx+1,5,WHITE);
      display.drawLine(hx+3,3,hx+1,5,WHITE);
    }
    display.display();
  }
}
// ══════════════════════════════════════════════════════════════════════
//  GAME 3 — SNAKE
// ══════════════════════════════════════════════════════════════════════
#define SN_CELL   4
#define SN_COLS  (SCREEN_W/SN_CELL)   // 32
#define SN_ROWS  (SCREEN_H/SN_CELL)   // 16
#define SN_MAX   128

struct SnPt { int8_t x, y; };

SnPt          sn_body[SN_MAX];
int           sn_len;
int8_t        sn_dx, sn_dy;
int8_t        sn_ndx, sn_ndy;   // buffered next direction
int           sn_fx, sn_fy;
int           sn_bfx, sn_bfy;
bool          sn_bigActive;
unsigned long sn_bigTimer;
int           sn_score;
bool          sn_gameOver, sn_started;
unsigned long sn_lastMove;
int           sn_speed;

// ── place food, avoid snake body ──────────────────────────────────────
void sn_placeFood() {
  bool ok = false;
  while (!ok) {
    sn_fx = random(SN_COLS);
    sn_fy = random(SN_ROWS);
    ok = true;
    for (int i = 0; i < sn_len; i++)
      if (sn_body[i].x == sn_fx && sn_body[i].y == sn_fy) { ok = false; break; }
  }
}

void sn_placeBigFood() {
  sn_bfx = random(SN_COLS);
  sn_bfy = random(SN_ROWS);
  sn_bigActive = true;
  sn_bigTimer  = millis() + 5000;   // visible for 5 seconds
}

void sn_init() {
  sn_len = 4;
  for (int i = 0; i < sn_len; i++) {
    sn_body[i].x = SN_COLS / 2 - i;
    sn_body[i].y = SN_ROWS / 2;
  }
  sn_dx = 1;  sn_dy = 0;
  sn_ndx = 1; sn_ndy = 0;
  sn_score    = 0;
  sn_gameOver = false;
  sn_started  = false;
  sn_speed    = 180;
  sn_bigActive = false;
  sn_placeFood();
}

void runSnake() {
  int hiScore = loadHS(HS_SNAKE);
  sn_init();
  sn_lastMove = millis();

  while (true) {
    readButtons();

    // ── MENU SCREEN ───────────────────────────────────────────────
    if (!sn_started) {
      display.clearDisplay();
      display.setTextSize(2); display.setTextColor(WHITE);
      display.setCursor(34, 4); display.print("SNAKE");
      display.setTextSize(1);
      display.setCursor(16, 26); display.print("4 btns = 4 dirs");
      display.setCursor(16, 38); display.print("R=start  L=menu");
      if (hiScore > 0) {
        display.setCursor(20, 52); display.print("Best: "); display.print(hiScore);
      }
      display.display();
      if (pressR) { sn_started = true; sn_lastMove = millis(); }
      if (pressL) return;
      delay(50);
      continue;
    }

    // ── GAME OVER SCREEN ──────────────────────────────────────────
    if (sn_gameOver) {
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(40,  2); display.print("SNAKE");
      display.drawFastHLine(0, 12, SCREEN_W, WHITE);
      display.setCursor(10, 18); display.print("Score : "); display.print(sn_score);
      display.setCursor(10, 30); display.print("Best  : "); display.print(hiScore);
      if (sn_score >= hiScore && sn_score > 0) {
        display.setCursor(20, 42); display.print("NEW RECORD!");
      }
      display.drawFastHLine(0, 52, SCREEN_W, WHITE);
      display.setCursor(2,  56); display.print("R=restart  L=menu");
      display.display();
      if (pressR) { sn_init(); hiScore = loadHS(HS_SNAKE); sn_lastMove = millis(); }
      if (pressL) return;
      delay(80);
      continue;
    }

    // ── DIRECTION INPUT — buffer next turn, prevent 180 reversal ──
    if (pressF && sn_dy == 0)  { sn_ndx =  0; sn_ndy = -1; }
    if (pressB && sn_dy == 0)  { sn_ndx =  0; sn_ndy =  1; }
    if (pressL && sn_dx == 0)  { sn_ndx = -1; sn_ndy =  0; }
    if (pressR && sn_dx == 0)  { sn_ndx =  1; sn_ndy =  0; }

    // ── BIG FOOD TIMER ────────────────────────────────────────────
    if (sn_bigActive && millis() > sn_bigTimer)
      sn_bigActive = false;

    // Spawn big food randomly (~every 8 seconds on average)
    if (!sn_bigActive && random(500) == 0)
      sn_placeBigFood();

    // ── MOVE STEP ─────────────────────────────────────────────────
    unsigned long now = millis();
    if (now - sn_lastMove >= (unsigned long)sn_speed) {
      sn_lastMove = now;

      // Apply buffered direction
      sn_dx = sn_ndx;
      sn_dy = sn_ndy;

      // New head position — wrap through walls
      int8_t nx = (int8_t)((sn_body[0].x + sn_dx + SN_COLS) % SN_COLS);
      int8_t ny = (int8_t)((sn_body[0].y + sn_dy + SN_ROWS) % SN_ROWS);

      // Self collision (skip tail tip — it moves away same frame)
      bool hit = false;
      for (int i = 0; i < sn_len - 1; i++)
        if (sn_body[i].x == nx && sn_body[i].y == ny) { hit = true; break; }

      if (hit) {
        sn_gameOver = true;
        if (sn_score > hiScore) { hiScore = sn_score; saveHS(HS_SNAKE, hiScore); }
      } else {
        // Shift body — each segment takes the position of the one ahead
        for (int i = sn_len - 1; i > 0; i--)
          sn_body[i] = sn_body[i - 1];
        sn_body[0].x = nx;
        sn_body[0].y = ny;

        // Check food
        if (nx == sn_fx && ny == sn_fy) {
          sn_score++;
          if (sn_len < SN_MAX) sn_len++;   // GROW
          sn_placeFood();
          sn_speed = max(60, 180 - sn_score * 4);   // speed up
        }

        // Check big food
        if (sn_bigActive && nx == sn_bfx && ny == sn_bfy) {
          sn_score += 5;
          for (int g = 0; g < 3 && sn_len < SN_MAX; g++) sn_len++;  // GROW x3
          sn_bigActive = false;
          sn_speed = max(60, 180 - sn_score * 4);
        }
      }
    }

    // ── DRAW ──────────────────────────────────────────────────────
    display.clearDisplay();

    // Snake body — filled circle for head, smaller circles for body
    for (int i = 0; i < sn_len; i++) {
      int px = sn_body[i].x * SN_CELL + SN_CELL / 2;
      int py = sn_body[i].y * SN_CELL + SN_CELL / 2;
      if (i == 0) {
        display.fillCircle(px, py, SN_CELL / 2, WHITE);       // head — solid
      } else {
        display.fillCircle(px, py, SN_CELL / 2 - 1, WHITE);   // body — slightly smaller
      }
    }

    // Normal food — blinking square
    if ((millis() / 250) % 2 == 0)
      display.fillRect(sn_fx * SN_CELL, sn_fy * SN_CELL, SN_CELL, SN_CELL, WHITE);

    // Big food — flashing star shape
    if (sn_bigActive) {
      int bx = sn_bfx * SN_CELL + SN_CELL / 2;
      int by = sn_bfy * SN_CELL + SN_CELL / 2;
      if ((millis() / 150) % 2 == 0) {
        display.drawLine(bx - 3, by, bx + 3, by, WHITE);
        display.drawLine(bx, by - 3, bx, by + 3, WHITE);
        display.drawLine(bx - 2, by - 2, bx + 2, by + 2, WHITE);
        display.drawLine(bx + 2, by - 2, bx - 2, by + 2, WHITE);
      }
    }

    // Score top left
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(sn_score);

    // Big food countdown bar — top right
    if (sn_bigActive) {
      long remaining = sn_bigTimer - millis();
      int barW = (int)map(remaining, 0, 5000, 0, 30);
      display.drawRect(SCREEN_W - 32, 0, 32, 5, WHITE);
      display.fillRect(SCREEN_W - 32, 0, barW, 5, WHITE);
    }

    display.display();
    delay(10);
  }
}
// ══════════════════════════════════════════════════════════════════════
//  GAME 4 — MAZE ESCAPE
// ══════════════════════════════════════════════════════════════════════
#define MZ_COLS  16
#define MZ_ROWS   8
#define MZ_CW     8
#define MZ_CH     8

uint16_t mz_hWalls[MZ_ROWS+1]; // horizontal walls (bit=col)
uint16_t mz_vWalls[MZ_ROWS];   // vertical walls   (bit=col)

int mz_px,mz_py; // player pos (cell)
int mz_ex,mz_ey; // exit pos
int mz_level, mz_lives;
bool mz_gameOver, mz_won, mz_started;
unsigned long mz_timeLeft;
unsigned long mz_lastSec;
// Breadcrumb trail
bool mz_visited[MZ_ROWS][MZ_COLS];

// Simple recursive-backtracker maze generator
bool mz_cellVisited[MZ_ROWS][MZ_COLS];

void mz_removeWall(int r,int c,int dr,int dc){
  if (dr==-1) mz_hWalls[r]   &=~(1<<c);
  if (dr== 1) mz_hWalls[r+1] &=~(1<<c);
  if (dc==-1) mz_vWalls[r]   &=~(1<<c);
  if (dc== 1) mz_vWalls[r]   &=~(1<<(c+1));
}

void mz_carve(int r,int c){
  mz_cellVisited[r][c]=true;
  int dirs[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
  // shuffle
  for (int i=3;i>0;i--){
    int j=random(i+1);
    int tr=dirs[i][0],tc=dirs[i][1];
    dirs[i][0]=dirs[j][0]; dirs[i][1]=dirs[j][1];
    dirs[j][0]=tr;         dirs[j][1]=tc;
  }
  for (auto &d:dirs){
    int nr=r+d[0], nc=c+d[1];
    if (nr<0||nr>=MZ_ROWS||nc<0||nc>=MZ_COLS) continue;
    if (mz_cellVisited[nr][nc]) continue;
    mz_removeWall(r,c,d[0],d[1]);
    mz_carve(nr,nc);
  }
}

void mz_generate(){
  // All walls on
  for (int r=0;r<=MZ_ROWS;r++) mz_hWalls[r]=0xFFFF;
  for (int r=0;r< MZ_ROWS;r++) mz_vWalls[r]=0xFFFF;
  memset(mz_cellVisited,0,sizeof(mz_cellVisited));
  mz_carve(0,0);
}

int mz_timeForLevel(int lv){ return max(10, 30-lv*2); }

void mz_init(int lv){
  mz_generate();
  mz_px=0; mz_py=0;
  mz_ex=MZ_COLS-1; mz_ey=MZ_ROWS-1;
  mz_timeLeft=mz_timeForLevel(lv);
  mz_lastSec=millis();
  memset(mz_visited,0,sizeof(mz_visited));
  mz_visited[0][0]=true;
  mz_gameOver=false; mz_won=false;
}

bool mz_canMove(int r,int c,int dr,int dc){
  if (dr==-1) return !(mz_hWalls[r]   &(1<<c));
  if (dr== 1) return !(mz_hWalls[r+1] &(1<<c));
  if (dc==-1) return !(mz_vWalls[r]   &(1<<c));
  if (dc== 1) return !(mz_vWalls[r]   &(1<<(c+1)));
  return false;
}

void runMaze(){
  int hiLevel=loadHS(HS_MAZE);
  mz_level=1; mz_lives=3;
  mz_init(mz_level);
  mz_started=false;

  // Scroll offset (so 16x8 grid fits in 128x64 with top HUD)
  // Each cell 7px wide, 7px tall → 112x56 + 8px top HUD
  #define MZ_OX 0
  #define MZ_OY 8

  while(true){
    readButtons();
    if (pressL && !mz_started) return;

    if (!mz_started){
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(32,8);  display.print("MAZE ESCAPE");
      display.setCursor(4,22);  display.print("Reach the X marker!");
      display.setCursor(4,34);  display.print("3 lives  timed");
      display.setCursor(4,48);  display.print("R=start  L=menu");
      display.display();
      if (pressR) mz_started=true;
      delay(80); continue;
    }

    if (mz_gameOver){
      if (mz_level-1>hiLevel){ hiLevel=mz_level-1; saveHS(HS_MAZE,hiLevel); }
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(28,2);  display.print("MAZE ESCAPE");
      display.drawFastHLine(0,12,SCREEN_W,WHITE);
      display.setCursor(10,16); display.print("Level : "); display.print(mz_level-1);
      display.setCursor(10,26); display.print("Best  : "); display.print(hiLevel);
      display.drawFastHLine(0,50,SCREEN_W,WHITE);
      display.setCursor(2,54);  display.print("R=restart  L=menu");
      display.display();
      if (pressR){ mz_level=1; mz_lives=3; mz_init(mz_level); }
      if (pressL) return;
      delay(80); continue;
    }

    // Timer
    if (millis()-mz_lastSec>=1000){
      mz_lastSec+=1000;
      if (mz_timeLeft>0) mz_timeLeft--;
      if (mz_timeLeft==0){
        mz_lives--;
        if (mz_lives<=0){ mz_level=1; mz_lives=3; mz_gameOver=true; }
        else mz_init(mz_level);
        continue;
      }
    }

    // Move
    int nr=mz_py, nc=mz_px;
    if (pressF && mz_canMove(mz_py,mz_px,-1,0)) nr--;
    if (pressB && mz_canMove(mz_py,mz_px, 1,0)) nr++;
    if (pressL && mz_canMove(mz_py,mz_px, 0,-1)) nc--;
    if (pressR && mz_canMove(mz_py,mz_px, 0, 1)) nc++;
    mz_px=nc; mz_py=nr;
    mz_visited[mz_py][mz_px]=true;

    if (mz_px==mz_ex && mz_py==mz_ey){
      mz_level++;
      mz_init(mz_level);
      continue;
    }

    // Draw — cell size auto-fit
    int cw=(SCREEN_W)/MZ_COLS;   // 8
    int ch=(SCREEN_H-8)/MZ_ROWS; // 7
    display.clearDisplay();

    // Walls
    for (int r=0;r<MZ_ROWS;r++){
      for (int c=0;c<MZ_COLS;c++){
        int x=MZ_OX+c*cw, y=MZ_OY+r*ch;
        // top wall
        if (mz_hWalls[r]&(1<<c))
          display.drawFastHLine(x,y,cw,WHITE);
        // left wall
        if (mz_vWalls[r]&(1<<c))
          display.drawFastVLine(x,y,ch,WHITE);
      }
    }
    // Bottom border
    display.drawFastHLine(MZ_OX,MZ_OY+MZ_ROWS*ch,MZ_COLS*cw,WHITE);
    // Right border
    display.drawFastVLine(MZ_OX+MZ_COLS*cw,MZ_OY,MZ_ROWS*ch,WHITE);

    // Trail
    for (int r=0;r<MZ_ROWS;r++)
      for (int c=0;c<MZ_COLS;c++)
        if (mz_visited[r][c] && !(r==mz_py&&c==mz_px))
          display.drawPixel(MZ_OX+c*cw+cw/2, MZ_OY+r*ch+ch/2, WHITE);

    // Exit X
    int ex2=MZ_OX+mz_ex*cw+1, ey2=MZ_OY+mz_ey*ch+1;
    display.drawLine(ex2,ey2,ex2+cw-2,ey2+ch-2,WHITE);
    display.drawLine(ex2+cw-2,ey2,ex2,ey2+ch-2,WHITE);

    // Player
    display.fillRect(MZ_OX+mz_px*cw+1,MZ_OY+mz_py*ch+1,cw-2,ch-2,WHITE);

    // HUD
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0,0);  display.print("Lv"); display.print(mz_level);
    display.setCursor(40,0); display.print(mz_timeLeft); display.print("s");
    display.setCursor(80,0); display.print("Life:"); display.print(mz_lives);

    display.display();
    delay(16);
  }
}

// ══════════════════════════════════════════════════════════════════════
//  GAME 5 — FIGHTER JET
// ══════════════════════════════════════════════════════════════════════
#define FJ_MAX_BULLETS  8
#define FJ_MAX_ENEMIES  5
#define FJ_MAX_EBULLETS 10
#define FJ_MAX_PWRS     3

struct FJBullet { float x,y; bool active; };
struct FJEnemy  { float x,y; float vy; bool active; int hp; };
struct FJPower  { float x,y; bool active; int type; }; // 0=triple 1=shield 2=rapid

float fj_px, fj_py;
int   fj_life;           // 0-100
bool  fj_shield;
int   fj_shieldTimer;
bool  fj_triple;
int   fj_tripleTimer;
bool  fj_rapid;
int   fj_rapidTimer;
int   fj_score;
bool  fj_gameOver, fj_started;
int   fj_fireCD;
FJBullet fj_bullets[FJ_MAX_BULLETS];
FJBullet fj_eBullets[FJ_MAX_EBULLETS];
FJEnemy  fj_enemies[FJ_MAX_ENEMIES];
FJPower  fj_pwrs[FJ_MAX_PWRS];
unsigned long fj_lastFrame;
float fj_speed;

void fj_spawnEnemy(){
  for (int i=0;i<FJ_MAX_ENEMIES;i++){
    if (!fj_enemies[i].active){
      fj_enemies[i]={float(random(4,SCREEN_W-14)),float(-8),0.6f+fj_score*0.003f,true,1+(fj_score/20)};
      return;
    }
  }
}

void fj_fireBullet(float x,float y){
  for (int i=0;i<FJ_MAX_BULLETS;i++){
    if (!fj_bullets[i].active){
      fj_bullets[i]={x,y,true};
      return;
    }
  }
}

void fj_init(){
  fj_px=SCREEN_W/2-4; fj_py=SCREEN_H-16;
  fj_life=100; fj_score=0; fj_gameOver=false; fj_started=false;
  fj_shield=false; fj_shieldTimer=0;
  fj_triple=false; fj_tripleTimer=0;
  fj_rapid=false;  fj_rapidTimer=0;
  fj_fireCD=0; fj_speed=1.8f;
  for (int i=0;i<FJ_MAX_BULLETS;i++)  fj_bullets[i].active=false;
  for (int i=0;i<FJ_MAX_EBULLETS;i++) fj_eBullets[i].active=false;
  for (int i=0;i<FJ_MAX_ENEMIES;i++)  fj_enemies[i].active=false;
  for (int i=0;i<FJ_MAX_PWRS;i++)     fj_pwrs[i].active=false;
}

void fj_drawJet(int x,int y,bool enemy){
  if (!enemy){
    // Player jet (pointing up)
    display.fillTriangle(x+4,y,x,y+8,x+8,y+8,WHITE);
    display.fillRect(x+2,y+6,5,4,WHITE);
    display.drawPixel(x+4,y-1,WHITE); // nose
    if (fj_shield) display.drawCircle(x+4,y+4,7,WHITE);
  } else {
    // Enemy jet (pointing down)
    display.fillTriangle(x+4,y+8,x,y,x+8,y,WHITE);
    display.fillRect(x+2,y,5,4,WHITE);
  }
}
void runFighterJet(){
  int hiScore=loadHS(HS_JET);
  fj_init();
  fj_lastFrame=millis();

  while(true){
    if (millis()-fj_lastFrame<33) continue;
    fj_lastFrame=millis();
    readButtons();

    if (pressL && !fj_started) return;

    if (fj_gameOver){
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(24,2);  display.print("FIGHTER JET");
      display.drawFastHLine(0,12,SCREEN_W,WHITE);
      display.setCursor(10,16); display.print("Score : "); display.print(fj_score);
      display.setCursor(10,26); display.print("Best  : "); display.print(hiScore);
      display.drawFastHLine(0,50,SCREEN_W,WHITE);
      display.setCursor(2,54);  display.print("R=restart  L=menu");
      display.display();
      if (pressR){ fj_init(); hiScore=loadHS(HS_JET); }
      if (pressL) return;
      continue;
    }

    if (!fj_started){
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(24,8);  display.print("FIGHTER JET");
      display.setCursor(4,22);  display.print("L/R=move  UP=fire");
      display.setCursor(4,32);  display.print("Grab powerups!");
      display.setCursor(4,48);  display.print("R=start  L=menu");
      display.display();
      if (pressR) fj_started=true;
      continue;
    }

    // Move
    if (btnL && fj_px>0)            fj_px-=fj_speed;
    if (btnR && fj_px<SCREEN_W-10)  fj_px+=fj_speed;
    if (btnF && fj_py>8)             fj_py-=fj_speed;
    if (btnB && fj_py<SCREEN_H-12)  fj_py+=fj_speed;

    // Auto-fire
    if (fj_fireCD>0) fj_fireCD--;
    int fireRate = fj_rapid?4:10;
    if (fj_fireCD==0){
      fj_fireBullet(fj_px+4, fj_py);
      if (fj_triple){
        fj_fireBullet(fj_px,   fj_py+3);
        fj_fireBullet(fj_px+8, fj_py+3);
      }
      fj_fireCD=fireRate;
    }

    // Move bullets
    for (int i=0;i<FJ_MAX_BULLETS;i++){
      if (!fj_bullets[i].active) continue;
      fj_bullets[i].y-=4;
      if (fj_bullets[i].y<0) fj_bullets[i].active=false;
    }

    // Spawn enemy
    if (random(40)==0) fj_spawnEnemy();

    // Move enemies & fire
    for (int i=0;i<FJ_MAX_ENEMIES;i++){
      if (!fj_enemies[i].active) continue;
      fj_enemies[i].y+=fj_enemies[i].vy;
      // Enemy fire
      if (random(60)==0){
        for (int j=0;j<FJ_MAX_EBULLETS;j++){
          if (!fj_eBullets[j].active){
            fj_eBullets[j]={fj_enemies[i].x+4,fj_enemies[i].y+8,true};
            break;
          }
        }
      }
      if (fj_enemies[i].y>SCREEN_H) fj_enemies[i].active=false;
    }

    // Move enemy bullets
    for (int i=0;i<FJ_MAX_EBULLETS;i++){
      if (!fj_eBullets[i].active) continue;
      fj_eBullets[i].y+=3;
      if (fj_eBullets[i].y>SCREEN_H) fj_eBullets[i].active=false;
    }

    // Spawn powerup occasionally
    if (random(120)==0){
      for (int i=0;i<FJ_MAX_PWRS;i++){
        if (!fj_pwrs[i].active){
          fj_pwrs[i] = {
    float(random(4, SCREEN_W - 8)),
    0.0f,
    true,
    (int)random(3)
};
          break;
        }
      }
    }
    for (int i=0;i<FJ_MAX_PWRS;i++){
      if (!fj_pwrs[i].active) continue;
      fj_pwrs[i].y+=0.5f;
      if (fj_pwrs[i].y>SCREEN_H) fj_pwrs[i].active=false;
    }

    // Timers
    if (fj_shieldTimer>0){ fj_shieldTimer--; if (!fj_shieldTimer) fj_shield=false; }
    if (fj_tripleTimer>0){ fj_tripleTimer--; if (!fj_tripleTimer) fj_triple=false; }
    if (fj_rapidTimer >0){ fj_rapidTimer--;  if (!fj_rapidTimer)  fj_rapid =false; }

    // Collisions: player bullets vs enemies
    for (int b=0;b<FJ_MAX_BULLETS;b++){
      if (!fj_bullets[b].active) continue;
      for (int e=0;e<FJ_MAX_ENEMIES;e++){
        if (!fj_enemies[e].active) continue;
        if (fabs(fj_bullets[b].x-fj_enemies[e].x-4)<8 &&
            fabs(fj_bullets[b].y-fj_enemies[e].y-4)<8){
          fj_bullets[b].active=false;
          fj_enemies[e].hp--;
          if (fj_enemies[e].hp<=0){ fj_enemies[e].active=false; fj_score+=10; }
        }
      }
    }

    // Enemy bullets vs player
    for (int i=0;i<FJ_MAX_EBULLETS;i++){
      if (!fj_eBullets[i].active) continue;
      if (fabs(fj_eBullets[i].x-fj_px-4)<6 && fabs(fj_eBullets[i].y-fj_py-4)<8){
        fj_eBullets[i].active=false;
        if (!fj_shield) fj_life-=10;
      }
    }

    // Enemy vs player (ram)
    for (int i=0;i<FJ_MAX_ENEMIES;i++){
      if (!fj_enemies[i].active) continue;
      if (fabs(fj_enemies[i].x-fj_px)<8 && fabs(fj_enemies[i].y-fj_py)<8){
        fj_enemies[i].active=false;
        if (!fj_shield) fj_life-=20;
      }
    }

    // Powerup pickup
    for (int i=0;i<FJ_MAX_PWRS;i++){
      if (!fj_pwrs[i].active) continue;
      if (fabs(fj_pwrs[i].x-fj_px)<10 && fabs(fj_pwrs[i].y-fj_py)<10){
        fj_pwrs[i].active=false;
        if      (fj_pwrs[i].type==0){ fj_triple=true;  fj_tripleTimer=300; }
        else if (fj_pwrs[i].type==1){ fj_shield=true;  fj_shieldTimer=200; }
        else                        { fj_rapid=true;   fj_rapidTimer=250;  }
      }
    }

    if (fj_life<=0){
      fj_gameOver=true;
      if (fj_score>hiScore){ hiScore=fj_score; saveHS(HS_JET,hiScore); }
      continue;
    }

    // Draw
    display.clearDisplay();

    // Enemies
    for (int i=0;i<FJ_MAX_ENEMIES;i++)
      if (fj_enemies[i].active)
        fj_drawJet((int)fj_enemies[i].x,(int)fj_enemies[i].y,true);

    // Player bullets
    for (int i=0;i<FJ_MAX_BULLETS;i++)
      if (fj_bullets[i].active)
        display.fillRect((int)fj_bullets[i].x,(int)fj_bullets[i].y,2,4,WHITE);

    // Enemy bullets
    for (int i=0;i<FJ_MAX_EBULLETS;i++)
      if (fj_eBullets[i].active)
        display.drawFastVLine((int)fj_eBullets[i].x,(int)fj_eBullets[i].y,4,WHITE);

    // Powerups
    for (int i=0;i<FJ_MAX_PWRS;i++){
      if (!fj_pwrs[i].active) continue;
      int px2=(int)fj_pwrs[i].x, py2=(int)fj_pwrs[i].y;
      display.drawRect(px2,py2,7,7,WHITE);
      char c= fj_pwrs[i].type==0?'3': fj_pwrs[i].type==1?'S':'R';
      display.setCursor(px2+1,py2); display.print(c);
    }

    // Player
    fj_drawJet((int)fj_px,(int)fj_py,false);

    // HUD
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0,0); display.print(fj_score);
    // Life bar
    int barW=40;
    display.drawRect(SCREEN_W-barW-2,0,barW+2,6,WHITE);
    display.fillRect(SCREEN_W-barW-1,1,(fj_life*barW)/100,4,WHITE);
    // Power indicators
    if (fj_triple) { display.setCursor(50,0); display.print("3x"); }
    if (fj_rapid)  { display.setCursor(64,0); display.print("RR"); }
    if (fj_shield) { display.setCursor(78,0); display.print("SH"); }

    display.display();
  }
}

// ══════════════════════════════════════════════════════════════════════
//  SETUP & LOOP
// ══════════════════════════════════════════════════════════════════════
void setup(){
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(PIN_F, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_L, INPUT_PULLUP);
  pinMode(PIN_R, INPUT_PULLUP);

  randomSeed(analogRead(0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("OLED fail"); while(true);
  }
  display.clearDisplay(); display.display();

  showBoot();
}

void loop(){
  runMenu();

  switch(currentGame){
    case GAME_FLAPPY: runFlappyEngine(false); break;
    case GAME_BRICK:  runBrickBlast();        break;
    case GAME_SNAKE:  runSnake();             break;
    case GAME_MAZE:   runMaze();              break;
    case GAME_JET:    runFighterJet();        break;
    case GAME_PLANE:  runFlappyEngine(true);  break;
  }
}