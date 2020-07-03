#include <Arduino.h>
/*-----( Import needed libraries )-------------------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/*-----( Declare Constants )-------------------------------------------------------------------------------------------------------*/
int DropPh = A0;              //pin that turns on pump motor with base solution
int IncreasePh = A1;          //pin that turns on pump motor with acid solution
int pHpin = A3;               //pin that sends signals to pH meter
float offset = 2.97;          //the offset to account for variability of pH meter
float offset2 = 0;            //offset after calibration
float slope = 0.59;           //slope of the calibration line
int fillTime = 10;            //time to fill pump tubes with acid/base after cleaning... pumps at 1.2 mL/sec
int delayTime = 10;           //time to delay between pumps of acid/base in seconds
int smallAdjust = 1;          //number of seconds to pump in acid/base to adjust pH when pH is off by 0.3-1 pH
int largeAdjust = 3;          //number of seconds to pump in acid/base to adjust pH when pH is off by > 1 pH
int negative = 0;             //indicator if the calibration algorithm should be + or - the offset (if offset is negative or not)

/*-----( Declare objects )---------------------------------------------------------------------------------------------------------*/


/*-----( Declare Variables )-------------------------------------------------------------------------------------------------------*/
float pHvalue;                //reads the voltage of the pH probe
float pHregulate;             //holds value that the user wants the pH to stay at throughout reaction
int regulatorPump;            //Pump in use to increase or drop the ph value
float deltaPH;                //holds the value of the difference between pHregulate and pHvalue
float pH4val;                 //value for calibration at pH4
float pH7val;                 //value for calibration at pH7
float pH10val;                //value for calibration at pH10
int pHavg[10];                //array to find an average pH of 10 meter readings
int temp;                     //temporary place holder used to sort array from small to large
unsigned long int avgValue;   //stores the average value of the 6 middle pH array readings






float CalibrationEC=1.38; //EC value of Calibration solution is s/cm

int R1= 1000; //Do not Replace R1 with a resistor lower than 300 ohms
int Ra=25; //Resistance of powering Pins
int ECPin= A1;
int ECGround=A2;
int ECPower =A3;

// Hana      [USA]        PPMconverion:  0.5
// Eutech    [EU]          PPMconversion:  0.64
//Tranchen  [Australia]  PPMconversion:  0.7
float PPMconversion=0.7;

float TemperatureCoef = 0.019; //this changes depending on what chemical we are measuring

float K=2.88; //Cell Constant For Ec Measurements

#define ONE_WIRE_BUS 8              
const int TempProbePossitive = 10;  
const int TempProbeNegative = 9;    

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// variables for calibration
float TemperatureFinish=0;
float TemperatureStart=0;

float Temperature=10;
float EC=0;
float EC25 =0;
int ppm =0;

float raw= 0;
float Vin= 5;
float Vdrop= 0;
float Rc= 0;

int i=0;
float buffer=0;

bool ECcalibrated = false;

// ph code
const int analogInPin = A0; 
int sensorValue = 0; 
unsigned long int avgValue;
float phValue = 0.0;
float b;
int buf[10],temp;

//ph calibration

#include <SoftwareSerial.h>                           //we have to include the SoftwareSerial library, or else we can't use it
#define rx 2                                          //define what pin rx is going to be
#define tx 3                                          //define what pin tx is going to be

SoftwareSerial myserial(rx, tx);                      //define how the soft serial port is going to work


String inputstring = "";                              //a string to hold incoming data from the PC
String sensorstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
float pH;   

void setup()
{
  Serial.begin(9600);
  pinMode(DropPh, OUTPUT);
  pinMode(IncreasePh, OUTPUT);


  pinMode(TempProbeNegative , OUTPUT ); //seting ground pin as output for tmp probe
  digitalWrite(TempProbeNegative , LOW );//Seting it to ground so it can sink current
  pinMode(TempProbePossitive , OUTPUT );//ditto but for positive
  digitalWrite(TempProbePossitive , HIGH );
  pinMode(ECPin,INPUT);
  pinMode(ECPower,OUTPUT);//Setting pin for sourcing current
  pinMode(ECGround,OUTPUT);//setting pin for sinking current
  digitalWrite(ECGround,LOW);//We can leave the ground connected permanantly

  delay(100);// gives sensor time to settle
  sensors.begin();
  delay(100);
  //** Adding Digital Pin Resistance to [25 ohm] to the static Resistor *********//
  // Consule Read-Me for Why, or just accept it as true
  R1=(R1+Ra);// Taking into acount Powering Pin Resitance

}



void readPH() /*--(Subroutine, reads current value of pH Meter)-----------------------------------------------------------------*/
{
  for (int i = 0; i < 10; i++)
  {
    pHavg[i] = analogRead(pHpin);
    delay(10);
  }

  for (int i = 0; i < 9; i++)
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (pHavg[i] > pHavg[j])
      {
        temp = pHavg[i];
        pHavg[i] = pHavg[j];
        pHavg[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++)
  {
    avgValue += pHavg[i];
  }
  pHvalue = (float)avgValue * 5.0 / 1024 / 6;

  if (negative == 0)
  {
    pHvalue = (slope * 3.5 * pHvalue + offset + offset2);
  }
  else
  {
    pHvalue = (slope * 3.5 * pHvalue - offset + offset2);
  }
}

void regulatePH(float DesiredPH)
{
  readPH();
  deltaPH = (DesiredPH - pHvalue);

  if (deltaPH < 0)
  {
    deltaPH = deltaPH * (-1);
    regulatorPump = DropPh;

  }
  else
  {
    regulatorPump = IncreasePh;
  }
  
recheck:
  if (deltaPH > 0.3)
  {
    digitalWrite(regulatorPump, HIGH);
    if (deltaPH > 1)
    {
      for (int i = 0; i < largeAdjust; i++)
      {
        readPH();

        deltaPH = abs(DesiredPH - pHvalue);
        if (deltaPH < 1)
        {
          digitalWrite(regulatorPump, LOW);
          goto recheck;
        }
        delay(1000);
      }
    }
    else //if difference in pH is between 0.3 and 1
    {
      for (int i = 0; i < smallAdjust; i++) //loop for smaller pH adjust time
      {
        readPH();
        deltaPH = abs(DesiredPH - pHvalue);
        if (deltaPH < 0.3)
        {
          digitalWrite(regulatorPump, LOW);
          goto recheck;
        }
        delay(1000);
      }
    }

    digitalWrite(regulatorPump, LOW);

    for (int i = 0; i < delayTime * 4; i++) //delay "delayTime" seconds to allow new solution to mix
    {
      delay(1000);
    }
  }
}

void CleanPump(int pump) /*--(Subroutine, cleans pump w/ DI water)--*/
  {
    //print out instructions for cleaning pump before use
    //wait for user input confirmation
    delay(500);
    digitalWrite(pump, HIGH);
    for (int i = 0; i < 15; i++)
    {
      delay(1000);
    }
  digitalWrite(pump, LOW);
  //print out instructions to clear ph meter of DI water
  //wait for user input confirmation
  digitalWrite(pump, HIGH);
  for(int i=0; i<10; i++)
    {
      delay(1000);
    }
  digitalWrite(pump, LOW);
}

void SetUpPump(int pump) /*--(Subroutine, fills pump with solution)--*/
{
  //print out instructions for setting up pump before use
  //wait for user input confirmation
  delay(500);
  digitalWrite(pump, HIGH);
  for (int i = 0; i < fillTime; i++)
  {
    delay(1000);
  }
  digitalWrite(pump, LOW);
}

void FlashWait()
{
  for (int i = 0; i < 3; i++)
  {
    delay(1000);
  }
}

void CalibratePhMeter() /*--(Subroutine, calibrates the pH meter using system of equations)----------------------------------------*/
{
  delay(50);
  //instruccions for inserting ph meter in 4ph solution
  //wait confirmation input from the user
  FlashWait();
  //calibration for ph4 starts
  for (double i = 100; i > 0; i--)
  {
    readPH();
    pH4val = pHvalue;
  }
  delay(50);
  //instrucctions for washing ph meter with di water
  //wait confirmation input from the user
  delay(50);
  //instruccions for inserting ph meter in 7ph solution
  //wait confirmation input from the user
  FlashWait();
  //calibration for ph7 starts
  for (double i = 100; i > 0; i--)
  {
    readPH();
    pH7val = pHvalue;
  }
  delay(50);
  //instrucctions for washing ph meter with di water
  //wait confirmation input from the user
  delay(50);
  //instruccions for inserting ph meter in 10ph solution
  //wait confirmation input from the user
  FlashWait();
  delay(50);
  //calibration for ph10 starts
  for (double i = 100; i > 0; i--)
  {
    readPH();
    pH10val = pHvalue;
  }
  delay(50);
  //instrucctions for washing ph meter with di water
  //wait confirmation input from the user
  delay(50);
  //instruccions for inserting ph meter in wanted solution
  //wait confirmation input from the user
  FlashWait();
  slope = 6 / (pH10val - pH4val);
  offset = (abs(11 - ((pH4val + pH7val) * slope))) / 2;
  offset2 = slope * 2.97;
  slope = 0.59 * slope;
  if ((pH4val + pH7val) > 11)
  {
    negative = 1;
  }
  else
  {
    negative = 0;
  }
}

void CalibrateEC()
{

  i=1;
  buffer=0;
  sensors.requestTemperatures();
  TemperatureStart=sensors.getTempCByIndex(0);

  while(i<=10)
  {
    digitalWrite(ECPower,HIGH);
    raw= analogRead(ECPin);
    raw= analogRead(ECPin);// This is not a mistake, First reading will be low
    digitalWrite(ECPower,LOW);
    buffer=buffer+raw;
    i++;
    delay(5000);
  }

  raw=(buffer/10);

  sensors.requestTemperatures();
  TemperatureFinish=sensors.getTempCByIndex(0);

  EC =CalibrationEC*(1+(TemperatureCoef*(TemperatureFinish-25.0))) ;

  Vdrop= (((Vin)*(raw))/1024.0);
  Rc=(Vdrop*R1)/(Vin-Vdrop);
  Rc=Rc-Ra;
  K= 1000/(Rc*EC);

  if (TemperatureStart==TemperatureFinish)
  {
    ECcalibrated = true;
  }
  else
  {
    ECcalibrated = false;
  }
}


void ReadEC(){
  sensors.requestTemperatures();
  Temperature=sensors.getTempCByIndex(0);

  digitalWrite(ECPower,HIGH);
  raw= analogRead(ECPin);
  raw= analogRead(ECPin);// This is not a mistake, First reading will be low beause if charged a capacitor
  digitalWrite(ECPower,LOW);

  Vdrop= (Vin*raw)/1024.0;
  Rc=(Vdrop*R1)/(Vin-Vdrop);
  Rc=Rc-Ra; //acounting for Digital Pin Resitance
  EC = 1000/(Rc*K);

  EC25  =  EC/ (1+ TemperatureCoef*(Temperature-25.0));
  ppm=(EC25)*(PPMconversion*1000);
  delay(5000);
}


void loop()
{
  if (!ECcalibrated)
  {
    CalibrateEC();
  }
  else
  {
    ReadEC();
  }
}