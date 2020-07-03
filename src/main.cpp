#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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

void setup()
{
  Serial.begin(9600);
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

void CalibrateEC()
{

  i=1;
  buffer=0;
  sensors.requestTemperatures();// Send the command to get temperatures
  TemperatureStart=sensors.getTempCByIndex(0); //Stores Value in Variable

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

  //*************Compensating For Temperaure********************//
  EC =CalibrationEC*(1+(TemperatureCoef*(TemperatureFinish-25.0))) ;

  //***************** Calculates R relating to Calibration fluid **************************//
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


void UpdateData(){
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
    UpdateData();
  }
}