#include"game_loop.h"

#include<Bounce2.h>

#include <Wire.h> // comes with arduino ide used for I2C communication
#include <Adafruit_GFX.h> // version 1.11.11, to draw (lines, texts, etc) on the OLED display
#include <Adafruit_SH110X.h>  // version 2.1.22, 1.3 inch OLED display driver

#define I2C_ADDRESS_OLED 0X3C // this address seems to work for me


#define PLAYER_1_CLK 2  // hardware interrupt pin
#define PLAYER_1_DT 8
#define PLAYER_1_BUTTON 4

#define PLAYER_2_CLK 3  // hardware interrupt pin
#define PLAYER_2_DT 16
#define PLAYER_2_BUTTON 15


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BALL_WIDTH 2
#define BALL_HEIGHT 2
#define BALL_SPEED 45

#define PADDLE_WIDTH 2
#define PADDLE_HEIGHT 16
#define PADDLE_STEP 2 // no. of steps the paddles are moved for each step of the rotary encoder


Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT); // main display object

Bounce2::Button button_1, button_2; // rotary encoder push buttons (used to activate AI players)

int score1 = 0, score2 = 0; // player scores, if your opponent misses you get 1 point

bool ai_p1 = false, ai_p2 = false;  // flags to activate AI


template<typename T>
struct coord2d
{
  T x, y;
};


// ===============================
//  Ball Class 
// ===============================


class BALL
{
  double x, y, dx, dy;

  public:

  BALL()
  {
    reset();  // reset the ball before the game begins
  }

  // reset ball to the center of the screen and give it a random speed and direction

  void reset()
  {
    x = SCREEN_WIDTH / 2;

    y = SCREEN_HEIGHT / 2;

    dx = random(2) ? BALL_SPEED : -BALL_SPEED;

		dy = random(2) ? (random(BALL_SPEED) + 10) : -(random(BALL_SPEED) + 10);
  }

  // update ball position

  void update(double dt)
  {
    x += dx * dt;
    
    y += dy * dt;

    // colliding with floor

		if(y >= SCREEN_HEIGHT - 1 - BALL_HEIGHT)
		{
			dy *= -1;	// changing direction of vertical velocity

			y = SCREEN_HEIGHT - 1 - BALL_HEIGHT;	// setting on the floor
		}

		// colliding with ceiling

		if(y <= 0)
		{
			dy *= -1;	// changing direction of vertical velocity

			y = 0;	// setting below the ceiling
		}
  }

  /*
    check if the ball collides with a paddle, if a collision is detected,
    place it on the colliding surface of the paddle, reverse its dx and
    randomly change it's dy
  */

  bool paddle_collides(coord2d<int16_t> paddle)
  {
    if (x > paddle.x + PADDLE_WIDTH || x + BALL_WIDTH < paddle.x)
        return false;

    if (y > paddle.y + PADDLE_HEIGHT || y + BALL_HEIGHT < paddle.y)
        return false;

    // collision resolve

    // collision is detected setting the ball on the front surface of the paddle

    if(dx < 0)
    {
      x = paddle.x + PADDLE_WIDTH;

      dx -= 3;	// increasing ball's horizontal velocity
    }
    else
    {
      x = paddle.x - BALL_WIDTH;

      dx += 3;	// increasing ball's horizontal velocity
    }

    dx *= -1;	// reversing the direction of ball's horizontal velocity

    dy = (dy >= 0) ? (random(BALL_SPEED) + 10) : -(random(BALL_SPEED) + 10);	// changing ball's vertical velocity

    return true;
  }

  /*
		indicate if the ball is out of bounds
		if its yes then shows which player has missed
	*/

	int8_t is_out_of_bounds()
	{
		if(x + BALL_WIDTH <= 0)
			return 2;	// player 2 scores as player 1 misses

		if(x >= SCREEN_WIDTH)
			return 1;	// player 1 scores as player 2 misses

		return 0;	// not out of bounds
	}

  coord2d<double> get_pos()
  {
    return {x, y};
  }

  // used to get the horizontal direction of the ball

  double get_dx()
  {
    return dx;
  }

  void render()
  {
    display.fillRect(x, y, BALL_WIDTH, BALL_HEIGHT, SH110X_WHITE);
  }
} ball;


// ===============================
//  Paddle Class 
// ===============================


class PADDLE
{
  /*
    these variables will be updated via a hardware interrupt, outside
    the flow of program logic, that's why they are volatile
  */
  
  volatile int16_t x, y, prev_clk, curr_clk;

  /*
    clk, dt - pins of the rotary encoder
    offset - used for ai
  */

  int8_t clk, dt, offset;

  public:

  PADDLE(int8_t clk_pin, int8_t dt_pin, int16_t x_pos) :
    
    clk(clk_pin),
    dt(dt_pin),
    x(x_pos),
    y(SCREEN_HEIGHT / 2 - PADDLE_HEIGHT / 2)
  
  {
    pinMode(clk, INPUT);
    pinMode(dt, INPUT);

    prev_clk = digitalRead(clk);

    reset_ai(); // reset the AI before the game begins
  }

  /*
    checks the rotary encoder inputs and updates paddle position, this function
    is not called inside the gameloop, instead a hardware interrupt is attached
    to the clk pin of the rotary encoder, whenever the voltage at clk changes
    it triggers the interrupt and executes this function

    I had to rely on hardware interrupts because in the gameloop, the render()
    takes a long time, as a result, rotary encoder input voltage changes get
    unnoticed, and the paddle starts to skip rotary encoder inputs
  */

  void input()
  {
    // reading the rotary encoder

    curr_clk = digitalRead(clk);

    if(curr_clk != prev_clk)
    {
        // rotary encoder has moved

        delay(5); // debounce delay

        // to get the direction we need to check the reading of DT pin against current clk

        if(curr_clk == digitalRead(dt))
        {
            y -= PADDLE_STEP;
        }
        else
        {
            y += PADDLE_STEP;
        }
    }

    prev_clk = curr_clk;

    // paddle touching top of the screen

    if(y < 0)
    {
      y = 0;
    }

    // paddle touching bottom of the screen

    if(y > SCREEN_HEIGHT - PADDLE_HEIGHT - 1)
    {
      y = SCREEN_HEIGHT - PADDLE_HEIGHT - 1;
    }
  }

  // safe way to get the paddle position

  coord2d<int16_t> get_pos()
  {
    coord2d<int16_t> pos;

    noInterrupts(); // pause the interrupts before we check the paddle position

    pos.x = x;

    pos.y = y;

    interrupts(); // resume the interrupts

    return pos;
  }

  // reset the paddle ai - generates a new offset for paddle ai

  void reset_ai()
	{
		offset = random(10) + 4;	// with less offset ai plays better
	}

  // uses position of the ball to autometically move the paddle

	void ai(coord2d<double> ball_pos)	// ai moves the paddles
	{
		if(ball_pos.y + BALL_HEIGHT / 2 < y + PADDLE_HEIGHT / 2 - offset)	// ball above the paddle
		{
			y -= PADDLE_STEP;

      // paddle touching top of the screen

      if(y < 0)
      {
        y = 0;
      }
		}
		else if(ball_pos.y + BALL_HEIGHT / 2 > y + PADDLE_HEIGHT / 2 + offset)  // ball below the paddle
		{
			y += PADDLE_STEP;

      // paddle touching bottom of the screen

      if(y > SCREEN_HEIGHT - PADDLE_HEIGHT - 1)
      {
        y = SCREEN_HEIGHT - PADDLE_HEIGHT - 1;
      }
		}
	}

  void render()
  {
    display.fillRect(x, y, PADDLE_WIDTH, PADDLE_HEIGHT, SH110X_WHITE);
  }
} paddle_1(PLAYER_1_CLK, PLAYER_1_DT, 0), paddle_2(PLAYER_2_CLK, PLAYER_2_DT, SCREEN_WIDTH - PADDLE_WIDTH - 1);


void Init() {
  // put your setup code here, to run once:

  // initialize and set up our display

  delay(250); // wait for our display to power up

  display.begin(I2C_ADDRESS_OLED);

  display.setContrast(10);  // dim the display

  // It's a monochrome display, so it's very obvious that I want white text

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  // initial screen

  display.clearDisplay();

  display.setCursor(SCREEN_WIDTH / 2 - 11, SCREEN_HEIGHT / 2 - 7);

  display.print("Pong");

  display.display();

  delay(1000); // wait for our display to get ready

  // setup the hardware interrupts for the rotary encoder inputs, when clk pin voltage changes we take paddle input

  attachInterrupt(digitalPinToInterrupt(PLAYER_1_CLK), [&paddle_1](){if(!ai_p1) paddle_1.input();}, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PLAYER_2_CLK), [&paddle_2](){if(!ai_p2) paddle_2.input();}, CHANGE);

  // initialize the buttons

  button_1.attach (PLAYER_1_BUTTON, INPUT_PULLUP);

  button_1.interval(5);
  
  button_1.setPressedState(LOW);

  button_2.attach (PLAYER_2_BUTTON, INPUT_PULLUP);

  button_2.interval(5);
  
  button_2.setPressedState(LOW);
  
  // seed for random number generator

  randomSeed(analogRead(0));
}


// checking the buttons

void Input()
{
  button_1.update();

  button_2.update();

  // toggle ai

  if(button_1.pressed())
  {
    ai_p1 = !ai_p1;
  }

  if(button_2.pressed())
  {
    ai_p2 = !ai_p2;
  }

  /*
    if ai is controlling a paddle, it moves the paddle when the ball is coming towards
    it, and ball's position is used to move the paddle
  */

  if(ai_p1 && ball.get_dx() < 0)
  {
    paddle_1.ai(ball.get_pos());
  }

  if(ai_p2 && ball.get_dx() > 0)
  {
    paddle_2.ai(ball.get_pos());
  }
}


// collision updates

void Update(double dt)
{
  ball.update(dt);

  // ball hits a paddle and bounces off

  ball.paddle_collides(paddle_1.get_pos());

  ball.paddle_collides(paddle_2.get_pos());

  // checking if a player misses the ball

  auto temp = ball.is_out_of_bounds();

  if(temp)
  {
    switch(temp)
    {
      case 1: score1++; break;

      case 2: score2++; break;
    }

    // ball is served from the center

    ball.reset();

    // ai gets new offset (when it's reset)

    paddle_1.reset_ai();

    paddle_2.reset_ai();
  }
}


void print_score1();

void print_score2();


void Render()
{
  display.clearDisplay();

  ball.render();

  paddle_1.render();

  paddle_2.render();

  print_score1();

  print_score2();

  display.display();
}


void print_score1()
{
  String num = String(score1);

  // getting height and width of the text

  int16_t x, y;

  uint16_t w, h;

  display.getTextBounds(num.c_str(), 0, 0, &x, &y, &w, &h);

  // place the text in the middle

  display.setCursor(SCREEN_WIDTH / 4 - w / 2, SCREEN_HEIGHT / 4);

  display.print(num.c_str()); // printing the number
}


void print_score2()
{
  String num = String(score2);

  // getting height and width of the text

  int16_t x, y;

  uint16_t w, h;

  display.getTextBounds(num.c_str(), 0, 0, &x, &y, &w, &h);

  // place the text in the middle

  display.setCursor(SCREEN_WIDTH - SCREEN_WIDTH / 4 - w / 2, SCREEN_HEIGHT / 4);

  display.print(num.c_str()); // printing the number
}