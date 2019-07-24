#include <Arduino.h>
#include <Wire.h> 
#include <MPU6050.h> 
#include <Adafruit_NeoPixel.h>
#include <../lib/mylib/myLib.h>
#include <../lib/mylib/src/gammacorr.h>

#include <WiFi.h>
#include <AsyncUDP.h>
const char * ssid = "fjerNet2";
const char * password = "kaffekande";


AsyncUDP udp;

int loop_ms;
int blink_count;
bool blink_state;
bool master = true;    
uint32_t slave_diff_ms = 0;
int slave_diff_ms_2nd = 0;
int master_time = 0;
int slave_time = 0;
int Error = 0;
String packet_time = "";


#define PIN 17
#define N_LEDS 6
#define OB_LED 5


uint32_t time_a, time_b, time_c;

MPU6050 mpu;
String outln;

//support functions
double Millis() {
  return ((double)micros())/1000;
}

template<typename T> T Abs(T x) {
  if(x<0) return -x; else return x;
}


struct thime {

  thime(bool ma, uint32_t* sla) 
   :master(ma), local(0), global(0), sla_dif(sla) {

   }
    
  void update() {
    if(sla_dif==NULL) return;

    local = (uint32_t)micros();
    if(master) {
      global = local;
    } else {
      global = local + *sla_dif;
    }
    Serial.println(*sla_dif);
  }

  //verion of millis outputting as double
  uint32_t millis() {return ((double)global)/1000;}

  bool master;      //Is this device master clock of the swarm
  uint32_t local;   //local time on device
  uint32_t global;  //global time of swarm
  uint32_t* sla_dif; 
  
};

struct thimer{
    thimer(const thime* tt, uint32_t tick_rate=100)
    : t(tt), tick_r(tick_rate), n_firings(0) {}
  
  bool is_even() {return t->local%2;}

  const thime* t;
  uint32_t n_firings;
  uint32_t tick_r;

  bool ready() {
    if(t->global >= n_firings*tick_r*1000) {
      ++n_firings;
      return true;
    } else {
      return false;
    }
  }

};

//Initialize objects

uint32_t* p = &slave_diff_ms;
thime TIME(true, p);
thimer led_thimer(&TIME,25);
thimer led_thimer_reset(&TIME,5000);
//thimer dimm_thimer(&TIME,25);

struct smooth_color_handler{
  
  Adafruit_NeoPixel neo = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);

  double r = 0;
  double g = 0;
  double b = 0;
  uint32_t neoColor{0};

  String outln;

  //ranges
  double imin = 0;
  double imax = 18000;
  double omin = 0;
  double omax = 255;

  //light correction
  double smooth_factor = 60;

  double mapcon(double x,bool usigned=true) {
    if(usigned) x = abs(x);
    x = min(max(x,imin),imax);
    x = (x-imin) / (imax-imin);
    x = x * omax + omin;
    return x;
  }

  void set(Vector acc) {
    r = (r*smooth_factor + mapcon((double)(int16_t)acc.XAxis))/(smooth_factor+1);
    g = (g*smooth_factor + mapcon((double)(int16_t)acc.YAxis))/(smooth_factor+1);
    b = (b*smooth_factor + mapcon((double)(int16_t)acc.ZAxis))/(smooth_factor+1);
    neoColor = neo.Color((int)r,(int)g,(int)b);
  };


  float pixx_state{0};
  void sapp() {
    //if(dimm_thimer.ready())
    pixx_state += 0.0087*5;
    if(pixx_state>=N_LEDS) pixx_state-=N_LEDS;
    double pf{0};
    //Serial.print("\n" + String(pixx_state) + "_");
    for(int i = 0; i<N_LEDS;i++) {
      pf = (((float)i)-pixx_state);
      pf = min(min(Abs(pf),Abs(pf+(double)N_LEDS)),Abs(pf-(double)N_LEDS));
      pf = pf/N_LEDS*2;
      pf = min(pf,1.0);
      pf = max(pf,0.0);
      neo.setPixelColor(i  , neo.Color(gc(r*pf),gc(g*pf),gc(b*pf)));
    }
  }

  void fill() {
    //if(dimm_thimer.ready())
    pixx_state += 0.0087*5;
    if(pixx_state>=N_LEDS) pixx_state-=N_LEDS;
    double pf{0};
    //Serial.print("\n" + String(pixx_state) + "_");
    for(int i = 0; i<N_LEDS;i++) {
      pf = (((float)i)-pixx_state);
      pf = min(min(Abs(pf),Abs(pf+(double)N_LEDS)),Abs(pf-(double)N_LEDS));
      pf = pf/N_LEDS*2;
      pf = min(pf,1.0);
      pf = max(pf,0.0);
      neo.setPixelColor(i  , neo.Color(gc(r*pf),gc(g*pf),gc(b*pf)));
    }
  }
  
  void send() {
    //portDISABLE_INTERRUPTS();
    delay(1);
    neo.show();
    //portENABLE_INTERRUPTS();
  }


  //void = fillsend

};

smooth_color_handler sRGB{};

void setup () {

  Serial.begin(115200);

  pinMode(OB_LED,OUTPUT);

  sRGB.neo.clear();
  sRGB.neo.begin();

  Serial.println("Initialize MPU6050");
  while(!mpu.begin(MPU6050_SCALE_2000DPS, MPU6050_RANGE_2G)) {
    Serial.println("Could not find a valid MPU6050 sensor, check wiring!");
    delay(500);
  }


  WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while(WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.print(". ");
        delay(20);
  }  

 
    
    if(udp.listenMulticast(IPAddress(239,1,2,3), 1234)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        
        udp.onPacket([](AsyncUDPPacket packet) {
            if(!master) {
              for(int i=0;i<packet.length();i++) packet_time =+ (char*)packet.data();
              slave_time = TIME.millis();
              master_time = packet_time.toInt();
              Serial.print("exp mtime:");
              Serial.print(slave_time+slave_diff_ms);
              Serial.print(", now mtime:");
              Serial.print(master_time);
              Serial.print(", error:");
              Error = slave_time+slave_diff_ms -  master_time;
              Serial.print(Error);
              Serial.print(", new diff:");
              slave_diff_ms =  master_time - slave_time;
              Serial.print(slave_diff_ms);
              Serial.print("\n");
            }

            
            //packet_time = packet.data();
            
            
            //if(!master) slave_diff_ms = String.toInt(packet.data());
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
            //reply to the client
            //packet.printf("Got %u bytes of data", packet.length());
        });
        //Send multicast
        //udp.print("Hello!");
    }

}

void loop() {
  TIME.update();
 // Read normalized values 



    if(master) {
      loop_ms = TIME.millis();
    } else {
      //loop_ms = TIME.millis();
      loop_ms = TIME.millis()+slave_diff_ms;
    }
    
    if(loop_ms >= blink_count * 1000) {
      blink_count++;
      blink_state = !blink_state;
      if(blink_state) {
        digitalWrite(OB_LED,HIGH);
      } else {
        digitalWrite(OB_LED,LOW);
      }

      if(master) udp.print(String(loop_ms));
    }
    //Send multicast
    
    Vector rawACC = mpu.readRawAccel();
    sRGB.set(rawACC);
    if(led_thimer.ready()) { //only write to 
    //if(led_thimer_reset.ready()) sRGB.pixx_state = 0;
    sRGB.sapp();
    delay(1);
    sRGB.send();
  }

}


