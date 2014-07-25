#define MIN_TILT 182.0
#define MAX_TILT 448.0
#define TILT_SWING 266.0

#define MIN_PAN 197.0
#define MAX_PAN 653.0
#define PAN_SWING 434.0


#define UP 'u'
#define DOWN 'd'
#define LEFT 'l'
#define RIGHT 'r'
#define CENTER_TILT 't'
#define CENTER_PAN 'p'

#define BIG_ADJUST 32

#include <Wire.h>
//Requires Adafruit servo driver
#include <Adafruit_PWMServoDriver.h>

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

//Expect Servos in slot 1 and slot 2
#define TILT_SERVO 1
#define PAN_SERVO 0

uint8_t user_tilt = 70;
uint8_t user_pan = 127;

char serialCmd;

int setTilt(uint8_t tilt)
{
  double pulse = MIN_TILT + (TILT_SWING*((double)tilt/255.0));
  Serial.println("Tilt: "); Serial.println(pulse);
  if(pulse < MIN_TILT) pulse = MIN_TILT;
  if(pulse > MAX_TILT) pulse = MAX_TILT; 
  //setServoPulse(TILT_SERVO, pulse);
  pwm.setPWM(TILT_SERVO, 0, pulse);
}

int setPan(uint8_t pan)
{
  double pulse = MIN_PAN + (PAN_SWING*((double)pan/255.0));
  Serial.println("Pan: "); Serial.println(pulse);
  if(pulse < MIN_PAN) pulse = MIN_PAN;
  if(pulse > MAX_PAN) pulse = MAX_PAN;
  //setServoPulse(PAN_SERVO, pulse);
  pwm.setPWM(PAN_SERVO, 0, pulse);
}



void adjust(char byteRecieved, uint8_t adjustment = 1)
{
  
  switch(byteRecieved)
  {
    case UP:
      user_tilt -= adjustment;
      break;
    case DOWN:
      user_tilt += adjustment;
      break;
    case LEFT:
      user_pan += adjustment;
      break;
    case RIGHT:
      user_pan -= adjustment;
      break;
    case CENTER_PAN:
      user_pan = 127;
      break;
    case CENTER_TILT:
      user_tilt = 127;
      break;
    default:
      if(adjustment == 1)
        adjust(byteRecieved+32, BIG_ADJUST);
      break;
  }
  setPan(user_pan);
  setTilt(user_tilt);
}


//Parse the web server
#include <SPI.h>
#include <Ethernet.h>


// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x33, 0x49 };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "camcommand.herokuapp.com";    // name address for Google (using DNS)
IPAddress ip(192,168,0,18);
EthernetClient client;

char timestamp[] = "0000000000";
char buffer[256];
uint8_t buffer_ptr = 0;
uint8_t command_ptr = 0;

void queryCommands()
{
  if (client.connect(server, 80)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.println("GET /command?timestamp=0 HTTP/1.1");
    client.println("Host: camcommand.herokuapp.com");
    client.println("Connection: close");
    client.println();
  } 
}


void setup()
{
  Serial.begin(9600);
  Serial.println("Centering");
  pwm.begin();
  pwm.setPWMFreq(60);
  setPan(user_pan);
  setTilt(user_tilt);
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  Serial.println("connecting...");

  queryCommands();
  
}
void loop()
{
    if (Serial.available() > 0) {
      // read the incoming byte:
      serialCmd = Serial.read();
      //Simple command routine
      adjust(serialCmd);
      delay(500);
     }
     // from the server, read them and print them:
    if (client.available()) {
      buffer[buffer_ptr] = client.read();
      if(buffer[buffer_ptr] == ':')
      {
        command_ptr = ++buffer_ptr;
      }
      else
      {
        ++buffer_ptr;
      }
    }
    // if the server's disconnected, stop the client:
  if (!client.connected()) {
    //Translate buffer
    
    //Reset
    buffer_ptr = 0;
    command_ptr = 0;
    client.stop();
    delay(500);
    queryCommands();
    
    
  }
}
