#ifndef GAME_LOOP_HEADER
#define GAME_LOOP_HEADER


// ===============================
//  Declaring Essential Functions 
// ===============================


// initializes the game

void Init();

// collects the inputs (called once per iteration of gameloop, usually checks for button presses)

void Input();

/*
  updates the game logic (called one or more times in each iteration of gameloop,
  gets the time interval between two update() calls in seconds, good for calculating
  collisions and other time dependent updates)
*/

void Update(double dt);

// renders the graphics (called according to the framerate, currently it's 100)

void Render();


// ===============================
//  Starting The Gameloop
// ===============================


void setup()
{
  Init(); // initialize the game
}


void loop()
{
  static unsigned long prev_tp = millis(), curr_tp, loop_t_accumulator = 0;

  static double t_accumulator = 0, step = 1.0 / 120.0; // update 60 times per sencond (step = 1 / 60 second)

  curr_tp = millis(); // getting current timepoint

  /*
    t_accumulator accumulates time between two loop() calls in seconds, which
    is sent to update() in 'steps'
  */

  t_accumulator += (curr_tp - prev_tp) / 1000.0;  // millisecond -> second

  prev_tp = curr_tp;  // updating previous timepoint

  Input();  // collects the inputs
  
  /*
    following loop calls update() untill t_accumulator is smaller than step, step is a constant,
    each call to update() gets a fixed duration "step", this is very essential for accurate
    collision detection
  */

  while(t_accumulator > step)
  {
    Update(step);

    t_accumulator -= step;
  }
  
  /*
    render() is called once every 10 milliseconds, that's 100 FPS

    instead of using delay, loop_t_accumulator gathers the time one loop() call takes
    and calls Render() when it's bigger than 10 milliseconds
  */
  
  loop_t_accumulator += millis() - curr_tp;

  if(loop_t_accumulator > 10)
  {
    Render();

    loop_t_accumulator = 0;
  }
}


#endif