#include <Arduino.h>


struct cubeState {
    cubeState() 
        : Time(0), ID(0), r(0), g(0), b(0) {
        Time = millis();
    }
    
    uint32_t Time;
    uint32_t ID;
    uint32_t command;
    uint32_t cmd_value;
    uint32_t i_state;
    uint32_t r;
    uint32_t g;
    uint32_t b;
    
    String toJSONstring();
    void fromJSONstring(String json);

    void setID(uint32_t x);
};

void printSome(String str);


struct thime {
  thime(bool ma, uint32_t of) 
   :master(ma), local(0), global(0), offset(of) {}
    
  //verion of millis outputting as double
  uint32_t millis() {return((double)global)/1000;}
  uint32_t  local;   //local time on device
  uint32_t  global;  //global time of swarm
  uint32_t  offset;  //difference from local to global. local + offset = global
  bool      master;  //difference from local to global. local + offset = global
  
  void update();
  void update(uint32_t);
};

struct thimer{
    thimer(const thime* tt, uint32_t tick_rate=100)
    : t(tt), tick_r(tick_rate), n_firings(0) {}
  
    bool is_even() {return t->local%2;}
    
    const thime* t;
    uint32_t n_firings;
    uint32_t tick_r;

    bool is_ready();
};

double us2s(uint32_t us);

