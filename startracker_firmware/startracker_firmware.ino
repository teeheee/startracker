/*
D13 sck
D12 do
d11 di
d10 ss
d9 dir
d8 step
d7 en
d2 ir
*/

#include <Arduino.h>
#include "PinDefinitionsAndMore.h"
#include <IRremote.h>
#include <TMC2130Stepper.h>
#include <CliTerminal.h>

#define EN_PIN    7  // Nano v3:	16 Mega:	38	//enable (CFG6)
#define DIR_PIN   9  //			19			55	//direction
#define STEP_PIN  8  //			18			54	//step
#define CS_PIN    10  //			17			64	//chip select
#define IR_PIN    2


#define FULL_STEPS_REVOLUTION 400UL
#define MICROSTEPPING 256UL
#define TRANSMISSION 8UL
#define STEPS_PER_REV (FULL_STEPS_REVOLUTION * MICROSTEPPING * TRANSMISSION)

#define MILLIS_PER_REV (1000UL * 60UL * 60UL * 24UL) //ms * sec * min * hour

TMC2130Stepper driver = TMC2130Stepper(EN_PIN, DIR_PIN, STEP_PIN, CS_PIN);
Cli_Terminal terminal(50);

unsigned long time_stamp = 0;
unsigned long extra_step_counter = 0;
unsigned long overall_time = MILLIS_PER_REV;
int shutter_time = 0;
int shutter_repeat = 0;
int shutter_active = 0;
unsigned long shutter_stamp = 0;

void shutter()
{  
  for (int i=0; i<3; i++)
  {       
    IrSender.sendSony(0xB4B8F, 20);    
    delay(40);  
  }
}

void step()
{
	digitalWrite(STEP_PIN, HIGH);
	delayMicroseconds(10);
	digitalWrite(STEP_PIN, LOW);
	delayMicroseconds(10);
}

unsigned long create_next_interval(unsigned long overall_time_ms, unsigned long cur_step)
{
  unsigned long overall_steps = STEPS_PER_REV;
	unsigned long step_time = overall_time_ms/overall_steps;
	unsigned long error_1 = overall_time_ms%overall_steps;
	if(cur_step % (overall_steps/error_1) == 0)
		step_time += 1;
	unsigned long error_2 = overall_steps%error_1;
	if(cur_step % (overall_steps/error_2) == 0)
		step_time -= 1;
	return step_time;
}

// shut [belichtungszeit] [wiederholungen]
void shutter_command_handler(String attribute)
{
  if(attribute.length() > 0)
  {
    int start = 0;
    int end = attribute.indexOf(' ');
    if(end > 0)
    {
      String belichtungszeit = attribute.substring(start,end);
      start = end;
      end = attribute.length();
      String wiederholungen = attribute.substring(start,end);
      shutter_time = belichtungszeit.toInt()*1000;
      shutter_repeat = wiederholungen.toInt();
      shutter_stamp = millis();
      Serial.print("shutter_time: ");
      Serial.println(shutter_time);
      Serial.print("shutter_repeat: ");
      Serial.println(shutter_repeat);
      Serial.print("shutter_stamp: ");
      Serial.println(shutter_stamp);
      return;
    }
    else
    {
      Serial.println("you need two parameters");
    }
  }
  else
  {
    Serial.println("attribute String to short");
  }
  Serial.println("usage: shut [belichtungszeit] [wiederholungen]");
}

void rotspeed_command_handler(String attribute)
{
  Serial.println("not impelemented");
}


void info_command_handler(String attribute)
{
  Serial.print("DRV_STATUS: ");
  Serial.println(driver.DRV_STATUS(),HEX);
  Serial.print("current_interval: ");
  Serial.println(create_next_interval(overall_time, extra_step_counter));
  Serial.print("time_stamp: ");
  Serial.println(time_stamp);
  Serial.print("extra_step_counter: ");
  Serial.println(extra_step_counter);
  Serial.print("overall_time: ");
  Serial.println(overall_time);
  Serial.print("shutter_time: ");
  Serial.println(shutter_time);
  Serial.print("shutter_repeat: ");
  Serial.println(shutter_repeat);
  Serial.print("shutter_stamp: ");
  Serial.println(shutter_stamp);
}

void setup() {
    Serial.begin(115200);
    IrSender.begin(IR_PIN, false); 
	  SPI.begin();
	  pinMode(MISO, INPUT_PULLUP);
    driver.begin(); 			// Initiate pins and registeries
    driver.rms_current(600); 	// Set stepper current to 600mA. The command is the same as command TMC2130.setCurrent(600, 0.11, 0.5);
    driver.stealthChop(1); 	// Enable extremely quiet stepping
    driver.microsteps(MICROSTEPPING);
    Serial.print("DRV_STATUS: ");
    Serial.println(driver.DRV_STATUS(),HEX);
	  digitalWrite(EN_PIN, LOW);
		driver.shaft_dir(1);
  #ifdef TEST
    unsigned long sum = 0;
    for(unsigned long steps = 0; steps < STEPS_PER_REV; steps++)
    {
        sum += create_next_interval(overall_time, steps);
    }
    Serial.println("-----All in ms------");
    Serial.print("real: ");
    Serial.println(sum);
    Serial.print("expect: ");
    Serial.println(MILLIS_PER_REV);
    Serial.print("deviation: ");
    if(sum > MILLIS_PER_REV)
      Serial.println(sum-MILLIS_PER_REV);
    else
      Serial.println(MILLIS_PER_REV-sum);
  #endif
    terminal.commands[0] = Command("shut", shutter_command_handler);
    terminal.commands[1] = Command("rot", rotspeed_command_handler);
    terminal.commands[2] = Command("info", info_command_handler);

    Serial.print(">");
    Serial.flush();
}


void loop() {
    terminal.cli_cal(); 

    unsigned long cur = millis();
    //handle shutter
    if(cur-shutter_stamp > shutter_time && shutter_active) //close
    {
      Serial.println("shutter close");
      shutter();
      shutter_active = 0;
      shutter_repeat--;
      shutter_stamp = cur;
    }
    if(cur-shutter_stamp > shutter_time && shutter_repeat > 0 && shutter_active==0) //open
    {
      Serial.println("shutter open");
      shutter();
      shutter_active = 1;
      shutter_stamp = cur;
    }

    //handle stepper
    unsigned long comp = create_next_interval(overall_time, extra_step_counter);
    if(cur-time_stamp > comp)
    {
      step();
      extra_step_counter++;
      time_stamp = cur;
    }
}
