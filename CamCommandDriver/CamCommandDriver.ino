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

#define DEFAULT_ADJUST 4
#define BIG_ADJUST 32

//Expect Servos in slot 1 and slot 2
#define TILT_SERVO 1
#define PAN_SERVO 0

#define TILT_CENTER 70
#define PAN_CENTER 127

#define DISCARD_INPUT_STATE 0
#define TIME_READ_STATE 1
#define COMMAND_READ_STATE 2


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
char command_buffer;

uint8_t time_ptr = 0;
uint32_t current_time = 0;
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
      user_pan += adjustment;
      break;
    case RIGHT:
      user_pan -= adjustment;
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

void HTTP(char *request)
{
  if (client.connect(server, 80)) {
#ifdef DEBUG
    Serial.println("Sending Query");
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
}

void queryCommands()
{
  char buffer[256];
  // Make a HTTP request:
  sprintf(buffer, "GET /commands/list.txt?timestamp=%u HTTP/1.1", current_time);
  HTTP(buffer); 
}

void clearCommands()
{
  HTTP("GET /commands/clear?confirm=yes HTTP/1.1");
  if(client.connected())
  {
    client.flush();
    client.stop();
  }
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
  char data = 0;
  uint8_t state = DISCARD_INPUT_STATE;

  while(client.available() > 0)
  {
    data = client.read();
    if(data == -1)
      break;
    switch(state)
    {
      case DISCARD_INPUT_STATE:
        if(!isDigit(data))
        {
#ifdef DEBUG
          Serial.write(data);
          delay(10);
#endif
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
          delay(10);
#endif
          if(isDigit(data))
          {
            time_buffer[time_ptr++] = data;
          }else{
#ifdef DEBUG
            Serial.println("Invalid character");
#endif
            state = DISCARD_INPUT_STATE;
          }
        }else{
          //Intentionally discard data
          time_buffer[time_ptr] = '\0';
          current_time = atoi(time_buffer);
          state = COMMAND_READ_STATE;
        }
        break;

      case COMMAND_READ_STATE:
        //We only need one character, just take the first one and discard the rest of the line
#ifdef DEBUG
            Serial.println("Command Accepted");
            Serial.println(data);
#endif
        command = data;
        //Execute command
        adjust(command);
        state = DISCARD_INPUT_STATE;
        break;

      default:
        break;
    }
  }
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
    clearCommands(); //Accumulate commands  
    delay(1000);
  }else{
    queryCommands();
  }
}
