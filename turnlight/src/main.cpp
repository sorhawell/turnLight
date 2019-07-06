#include <Arduino.h>
#include <Wire.h> 
#include <MPU6050.h> 
#include <Adafruit_NeoPixel.h>


#define PIN 17
#define N_LEDS 96
int hue{0};
int sat{200};
int time_a, time_b;
uint32_t stripColor{0};
MPU6050 mpu;
String outln;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);

double mapcon(double x,double imin,double imax,double omin,double omax) {
  Serial.println("\ni:"+String(x)+ " "); 
  x = min(max(x,imin),imax);
  Serial.println("c:"+String(x)+ " ");  
  x = (x-imin) / (imax-imin);
  Serial.println("r:"+String(x)+ " ");  
  x = x * 254;
  Serial.println("o:"+String(x)+ " ");  
  return x;
}

struct smooth_color_handler{
  double r = 0;
  double g = 0;
  double b = 0;

  String outln;

  double imin = 0;
  double imax = 20000;
  double omin = 0;
  double omax = 255;

  double smooth_factor = 25;

  void set(double x, double y, double z) {
    r = (smooth_factor*r + mapcon(x,imin,imax,omax,omin))/(smooth_factor+1);
    g = (smooth_factor*g + mapcon(y,imin,imax,omax,omin))/(smooth_factor+1);
    b = (smooth_factor*b + mapcon(z,imin,imax,omax,omin))/(smooth_factor+1);

    //double val = (r+g+b)/3;



/*     r=r/val*100;
    g=g/val*100;
    b=b/val*100; */

    outln  ="xyz are:" + String(x) + " " +  String(y) + " " +  String(z) + "\n";
    outln +="rgb are:" + String(r) + " " +  String(g) + " " +  String(b) + "\n";
    Serial.println(outln);
  };
  
};

smooth_color_handler sRGB{};


void setup () {
  Serial.begin(115200);
  
  strip.clear();
  strip.begin();

  Serial.println("Initialize MPU6050");

  while(!mpu.begin(MPU6050_SCALE_2000DPS, MPU6050_RANGE_2G))
  {
    Serial.println("Could not find a valid MPU6050 sensor, check wiring!");
    delay(500);
  }
}

void loop() {
  time_a = millis();
  
  // Read normalized values 
  Vector normAccel = mpu.readRawAccel();
  //Serial.println(hue);

  Serial.println((int16_t)normAccel.XAxis);
  Serial.println((int16_t)normAccel.YAxis);
  Serial.println((int16_t)normAccel.ZAxis);

  sRGB.set((double)(int16_t)normAccel.XAxis,(double)(int16_t)normAccel.YAxis,(double)(int16_t)normAccel.ZAxis);
  
  stripColor = strip.Color((int)sRGB.r,(int)sRGB.g,(int)sRGB.b);
  strip.fill(stripColor,0,N_LEDS-1);
  strip.show();

  time_b = millis();
  outln = "time is: " + String(time_b-time_a);

  Serial.println(outln);
  delay(5); 
}

