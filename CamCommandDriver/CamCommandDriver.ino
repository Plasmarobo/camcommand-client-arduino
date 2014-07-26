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

#define DEBUG
#define DEBUG_DELAY 10

#define DEFAULT_ADJUST 8
#define BIG_ADJUST 64

//Expect Servos in slot 1 and slot 2
#define TILT_SERVO 1
#define PAN_SERVO 0

#define TILT_CENTER 70
#define PAN_CENTER 127

#define DETECT_TIME_STATE 0
#define TIME_READ_STATE 1
#define COMMAND_READ_STATE 2
#define DISCARD_LINE_STATE 3

#define NETWORK_TIME_PAD 150

#include <Wire.h>
//Requires Adafruit servo driver
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <Ethernet.h>

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x33, 0x49 };
char server[] = "camcommand.herokuapp.com";    
IPAddress ip(192,168,0,18);
EthernetClient client;


uint8_t user_tilt = 70;
uint8_t user_pan = 127;

char serialCmd;

char time_buffer[256];

uint8_t time_ptr = 0;
long current_time = 0;
boolean last_connected = false;

int setTilt(uint8_t tilt)
{
  double pulse = MIN_TILT + (TILT_SWING*((double)tilt/255.0));
#ifdef DEBUG
  Serial.println("Tilt: "); Serial.println(pulse);
#endif
  if(pulse < MIN_TILT) pulse = MIN_TILT;
  if(pulse > MAX_TILT) pulse = MAX_TILT; 
  //setServoPulse(TILT_SERVO, pulse);
  pwm.setPWM(TILT_SERVO, 0, pulse);
}

int setPan(uint8_t pan)
{
  double pulse = MIN_PAN + (PAN_SWING*((double)pan/255.0));
#ifdef DEBUG
  Serial.println("Pan: "); Serial.println(pulse);
#endif
  if(pulse < MIN_PAN) pulse = MIN_PAN;
  if(pulse > MAX_PAN) pulse = MAX_PAN;
  //setServoPulse(PAN_SERVO, pulse);
  pwm.setPWM(PAN_SERVO, 0, pulse);
}

void adjust(char byteRecieved, uint8_t adjustment = DEFAULT_ADJUST)
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
      user_pan -= adjustment;
      break;
    case RIGHT:
      user_pan += adjustment;
      break;
    case CENTER_PAN:
      user_pan = PAN_CENTER;
      break;
    case CENTER_TILT:
      user_tilt = TILT_CENTER;
      break;
    default:
      if(adjustment == DEFAULT_ADJUST)
        adjust(byteRecieved+32, BIG_ADJUST);
      break;
  }
  setPan(user_pan);
  setTilt(user_tilt);
}

void HTTP(char* request)
{
  if (client.connect(server, 80)) {
#ifdef DEBUG
    Serial.println("HTTP Request");
    Serial.println(request);
#endif
    client.println(request);
    client.println("Host: camcommand.herokuapp.com");
    client.println("User-agent: camcommand-client-arduino");
    client.println("Connection: close");
    client.println();
  }else{
#ifdef DEBUG
    Serial.println("Connection failed");
#endif
    client.stop();
  }
  delay(NETWORK_TIME_PAD);
}

void queryCommands()
{
#ifdef DEBUG
    Serial.println("queryCommands");
#endif
  char buffer[256];
  // Make a HTTP request:
  sprintf(buffer, "GET /commands/list.txt?time=%lu HTTP/1.1", current_time);
  HTTP(buffer); 
}

boolean connectionReady()
{
  boolean ready = client.connected();
  if (!ready && last_connected) {
#ifdef DEBUG
    Serial.println("Disconnecting.");
#endif
    client.stop();
  }
  last_connected = ready;
  return ready;
}

boolean isDigit(char data)
{
  return (data >= '0') && (data <= '9');
}

void parseCommandStream()
{
  //FSM model
  char data = -1;
  uint8_t state = DETECT_TIME_STATE;
  
  while(client.available() > 0)
  {
    data = client.read();
    
    if(data == -1)
      break;
      
    switch(state)
    {
      
      case DETECT_TIME_STATE:
        if(!isDigit(data))
        {
#ifdef DEBUG
          Serial.write(data);
          delay(DEBUG_DELAY);
#endif
          state = DISCARD_LINE_STATE;
          break;
        }else{
          time_ptr = 0;
          state = TIME_READ_STATE;
#ifdef DEBUG
          Serial.println("Reading Time");
#endif
        } //INTENTIONAL FALLTHROUGH

      case TIME_READ_STATE:
        if(data != ':')
        {
#ifdef DEBUG
          Serial.write(data);
          delay(DEBUG_DELAY);
#endif
          if(isDigit(data))
          {
            time_buffer[time_ptr++] = data;
          }else{
#ifdef DEBUG
            Serial.println("Invalid character");
#endif
            state = DISCARD_LINE_STATE;
          }
        }else{
          //Intentionally discard data
          time_buffer[time_ptr] = '\0';
          current_time = atol(time_buffer);
#ifdef DEBUG
            Serial.println("Read Time As");
            Serial.println(current_time);
#endif
          state = COMMAND_READ_STATE;
        }
        break;

      case COMMAND_READ_STATE:
        //We only need one character, just take the first one and discard the rest of the line
#ifdef DEBUG
            Serial.println("Command Accepted");
            Serial.println(data);
            delay(DEBUG_DELAY);
#endif
        //Execute command
        adjust(data);
        state = DETECT_TIME_STATE;
        break;
        
       case DISCARD_LINE_STATE:
         if (data == '\n')
          state = DETECT_TIME_STATE;
         break;

      default:
        time_ptr = 0;
        state = DETECT_TIME_STATE;
        break;
    }
  }
  client.stop();
  delay(250);
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("Centering");
#endif
  pwm.begin();
  pwm.setPWMFreq(60);
  setPan(user_pan);
  setTilt(user_tilt);
  
  if (Ethernet.begin(mac) == 0) {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
    Ethernet.begin(mac, ip);
  }
  delay(1000);
#ifdef DEBUG
  Serial.println("connecting...");
#endif
  queryCommands();  
}

void loop()
{
#ifdef DEBUG
  //Accept commands over serial if we are in debug mode
  if (Serial.available() > 0) {
    serialCmd = Serial.read();
    adjust(serialCmd);
  }
#endif
  if(connectionReady())
  {
    parseCommandStream();
  }else{
    queryCommands();
  }
}
