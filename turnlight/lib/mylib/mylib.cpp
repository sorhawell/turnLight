#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncUDP.h>

void printSome(String str) {
    Serial.print("some " + str);
}

/* template <typename T>
const char* makeMemCopy(const T* obj) {
    uint16_t sz = sizeof(obj);
    char b[sz];
    memcpy(b, obj, sz);
} */

struct cubeState
{
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

void cubeState::setID(uint32_t x) {
    ID = x;
}

String cubeState::toJSONstring() {
    DynamicJsonDocument doc(1024);
    //variables
    doc["c"]   = command;
    doc["v"] = cmd_value;
    doc["I"] = ID;
    doc["T"] = Time;
    doc["i"] = i_state;
    doc["r"] = r;
    doc["g"] = g;
    doc["b"] = b;
    String myStr = "";
    String& myref = myStr;  
    serializeJson(doc, myref);
    return myStr;
}

void cubeState::fromJSONstring(String json) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, json);
    Time = doc["T"];
    ID = doc["I"];
    command = doc["c"];
    cmd_value = doc["v"];
    i_state = doc["i"];
    r = doc["r"];
    g = doc["g"];
    b = doc["b"];
}


struct thime {
  thime(bool ma = true, uint32_t of = 0) 
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

void thime::update() {
    local = (uint32_t) micros();
    if (master) {
      global = local;
    } else {
      global = local + offset;
    }
}

void thime::update(uint32_t of) {
    offset = of;
    thime::update();
}

struct thimer{
    thimer(const thime* tt, uint32_t tick_rate=100)
    : t(tt), tick_r(tick_rate), n_firings(0) {}

    bool is_even() {return t->local%2;}
    const thime* t;
    uint32_t n_firings;
    uint32_t tick_r;
    bool is_ready();
};

bool thimer::is_ready() {
    if(t->global >= n_firings*tick_r*1000) {
        ++n_firings;
        return true;
    } else {
        return false;
    }
}


double us2s(uint32_t us) {
    return ((double)us)/1000.0;
}

/* void packet_print(AsyncUDPPacket packet) {
    Serial.print("UDP PT: ");
            Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
            Serial.print(", F: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", T: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", L: ");
            Serial.print(packet.length());
            Serial.print(", D: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();
} */



