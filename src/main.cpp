#include <Arduino.h>


/*-----( Import needed libraries )-------------------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/*-----( Declare Constants )-------------------------------------------------------------------------------------------------------*/
int DropPh = 9;              //pin that turns on pump motor with base solution
int IncreasePh = 10;          //pin that turns on pump motor with acid solution
int nutrientsPump = 11;            //pin that turns on pump motor wtih nutrients
int pHpin = A0;               //pin that sends signals to pH meter
float offset = 2.97;          //the offset to account for variability of pH meter
float offset2 = 0;            //offset after calibration
float slope = 0.59;           //slope of the calibration line
int fillTime = 10;            //time to fill pump tubes with acid/base after cleaning... pumps at 1.2 mL/sec
int delayTime = 10;           //time to delay between pumps of acid/base in seconds
int smallAdjust = 1;          //number of seconds to pump in acid/base to adjust pH when pH is off by 0.3-1 pH
int largeAdjust = 3;          //number of seconds to pump in acid/base to adjust pH when pH is off by > 1 pH
int negative = 0;             //indicator if the calibration algorithm should be + or - the offset (if offset is negative or not)
float Vin = 5;

#define ONE_WIRE_BUS 8              
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


float K = 0; //Cell Constant For Ec Measurements

// variables for EC calibration
float TemperatureFinish=0;
float TemperatureStart=0;

float Temperature = 0;
float EC = 0;
float EC25 = 0;
int ppm = 0;
float raw = 0; //raw value read by EC meter, this value is between 0 and 1024
float Vdrop = 0; //obtained voltage by ECpin
float Rc = 0;
int i=0;
float buffer=0;
bool ECcalibrated = false;


/*-----( Declare objects )---------------------------------------------------------------------------------------------------------*/
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd (0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  //set the LCD address to 0x20 for a 20 chars and 4 line display



void setup()
{
  Serial.begin(9600);
  pinMode(DropPh, OUTPUT);
  pinMode(IncreasePh, OUTPUT);


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

void tryNutrients(float min, float max, int iterations, float SegsInterval) //func for apply nutrients
{

  float MiliSegsDelay = int(SegsInterval * 1000)
  ReadEC();
recheck:
  if (EC25 < min)
  {
    digitalWrite(nutrientsPump, HIGH);
    delay(10);
    digitalWrite(nutrientsPump, LOW);

    for (int i = 0; i < iterations; i++)
    {
      readPH();

      if (EC25 > max)
      {
        digitalWrite(nutrientsPump, LOW);
        goto recheck;
      }
      delay(MiliSegsDelay);
    }
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
      for (int i = 0; i < smallAdjust; i++)
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

    for (int i = 0; i < delayTime * 4; i++)
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

void UpdatePhConst(float constPh) //function for setting PhConstants (ph4Val, ph7Val and ph10Val) before calibration
{
  delay(50);
  //instruccions for inserting ph meter in 4ph solution
  //wait confirmation input from the user
  //setting up constPh starts
  for (double i = 100; i > 0; i--)
  {
    readPH();
    constPh = pHvalue;
  }
  delay(50);
  //instrucctions for washing ph meter with di water
  //wait confirmation input from the user
}

void CalibratePhMeter() /*--(Subroutine, calibrates the pH meter using system of equations)----------------------------------------*/
{
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

float GetTemperature()
{
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void CalibrateEC(float ECsolution) //Ecsolutions refers to known EC in solution for calibration
{

recalibrate:
  i=1;
  buffer=0;

  TemperatureStart = GetTemperature()

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

  TemperatureFinish = GetTemperature()

  EC =ECsolution*(1+(TemperatureCoef*(TemperatureFinish-25.0))) ;

  Vdrop= (((Vin)*(raw))/1024.0);
  Rc=(Vdrop*R1)/(Vin-Vdrop);
  Rc=Rc-Ra;
  

  if (TemperatureStart==TemperatureFinish)
  {
    ECcalibrated = true;
    K= 1000/(Rc*EC);
  }
  else
  {
    ECcalibrated = false;
    goto recalibrate;
  }
}


void ReadEC(){

  if (K != 0)
  {
    Temperature= GetTemperature()

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
  else
  {
    //the EC meter hasnt been calibrated
  }
}

void ShowInLcd()
{
  ReadEC();
  readPH();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print('Temperature: ');

  lcd.setCursor(15, 0);
  lcd.print(GetTemperature());

  lcd.setCursor(0, 1);
  lcd.print('EC: ');

  lcd.setCursor(5, 1);
  lcd.print(EC25);

  lcd.setCursor(7, 1);
  lcd.print('Ph: ');

  lcd.setCursor(14, 1);
  lcd.print(pHvalue);
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