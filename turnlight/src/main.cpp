#include <Arduino.h>
#include <Wire.h> 
#include <MPU6050.h> 
#include <Adafruit_NeoPixel.h>
#include <../lib/mylib/myLib.h>
#include <../lib/gammacorr/src/gammacorr.h>

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

template<typename T> T Abs(T x) {
  if(x<0) return -x; else return x;
}


//Initialize objects

uint32_t p = slave_diff_ms;
thime TIME(true, 0);
thimer led_thimer(&TIME,15);

String myString = "";
String& myref = myString;

cubeState myCS{};
cubeState otherCS{};


int pings_sent = 0;
int pings_recieved = 0;
double packet_loss_ratio = 0;



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
      neo.setPixelColor(
        i, neo.Color(
          gc((r+otherCS.r)*pf/2),
          gc((g+otherCS.g)*pf/2),
          gc((b+otherCS.b)*pf/2)
        )
      );
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
    portDISABLE_INTERRUPTS();    
    neo.show();
    portENABLE_INTERRUPTS();
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
        Serial.print(" . ");
        delay(200);
  }  

  if(udp.listenMulticast(IPAddress(239,1,2,3), 1234)) {  
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    
    udp.onPacket([](AsyncUDPPacket packet) {
      myCS.Time = micros();
      myCS.i_state++;
      bool verbose = false;
      
      /*  String packet_msg = packet.readString();
      Serial.println("hurray we got some message"); */

      String packet_msg;
      for(int i=0;i<packet.length();i++) packet_msg =+ (char*)packet.data();
      if(verbose) Serial.println(packet_msg);
      
      otherCS.fromJSONstring(packet_msg);
      
      //handling commands
      if(otherCS.command==1) {
        if(verbose) Serial.println("cmd1 return ping with following state");
        myCS.command = 2;
        myCS.cmd_value = otherCS.cmd_value;
        udp.print(myCS.toJSONstring());
        if(verbose) Serial.println(myCS.toJSONstring());
      };

      if(otherCS.command==2) {
        myCS.command = 0;
        pings_recieved++;
        /* Serial.print("cmd2 eval ping| time diff: ");
        Serial.print(  us2s(otherCS.Time- myCS.Time));
        Serial.print("   latency: ");
        Serial.print(us2s(myCS.Time-otherCS.cmd_value));
        Serial.print("   abstime: ");
        Serial.println(us2s(myCS.Time)); */
      }

        if(otherCS.command==3) {
        myCS.command = 0;
        //Serial.println("colors recieved");
        //Serial.println(otherCS.r);
        //Serial.println(otherCS.g);
        //Serial.println(otherCS.b);
      }
      
      if(verbose) {
        Serial.println("udp info...");
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
        Serial.println("handling of this request took...");
        Serial.println(us2s(micros()-myCS.Time));
      }

    });
  }

}


thimer ping_timer(&TIME, 2000);
thimer emit_rgb_timer(&TIME, 25);

void loop() {
  TIME.update();

  if(led_thimer.is_ready()) { //only write to 
    sRGB.sapp();
    sRGB.send();
  }
  // Read normalized values 


  Vector rawACC = mpu.readRawAccel();
  sRGB.set(rawACC);



  if(ping_timer.is_ready()) {
      /* Serial.print("try pinging... sent packages: ");
      Serial.print(++pings_sent);
      Serial.print(" ratio: ");
      packet_loss_ratio = 1- (double)pings_recieved / pings_sent;
      Serial.println(packet_loss_ratio); */

      myCS.cmd_value = micros();
      myCS.i_state++;
      myCS.command = 1;
      udp.print(myCS.toJSONstring());
  }

  if(emit_rgb_timer.is_ready()) {
      //Serial.println("sending colors");
      
      myCS.i_state++;
      myCS.command = 3;
      myCS.r = sRGB.r;
      myCS.g = sRGB.g;
      myCS.b = sRGB.b;
      udp.print(myCS.toJSONstring());
  }

}


