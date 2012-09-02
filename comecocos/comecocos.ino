/*
  The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
 */
 
 /*
 COMECOCOS
 */
#include <LiquidCrystal.h>
#define FANTASMA   2
#define COCO       3
#define BOLA       1
#define VACIO      ' '
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
byte comecocos[8]={
  B00110,
  B01111,
  B11110,
  B11100,
  B11110,
  B01111,
  B00110,
  B00000
};
byte comida[8]={
  B00000,
  B00000,
  B00000,
  B00100,
  B00000,
  B00000,
  B00000,
  B00000
};
byte fantasma[8]={
  B01110,
  B11111,
  B10101,
  B11111,
  B11111,
  B10101,
  B00000,
  B00000
};
char linea1[16];
char linea2[16];
int pos;
int col;
int v=500;
void setup() {
  Serial.begin(9600);
  pinMode(8, INPUT);
  lcd.createChar(BOLA, comida);
  lcd.createChar(FANTASMA, fantasma);
  lcd.createChar(COCO, comecocos);
  lcd.begin(16, 2);
  
  lcd.clear();
  lcd.print("   Eloy-Cocos   ");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("  Hack:Cthulhu  ");
  delay(600);
  lcd.setCursor(0, 1);
  lcd.print(" .Hack:Cthulhu. ");
  delay(600);
  lcd.setCursor(0, 1);
  lcd.print(".:Hack:Cthulhu:.");
  delay(600);
  reset();
}
void reset()
{
  char resetL1[] = {3,1,1,2,1,1,1,1,1,1,2,1,1,1,1,1};
  char resetL2[] = {2,1,1,1,1,1,2,1,1,1,1,1,1,2,2,1};
  
  memcpy(linea1, resetL1, 16);  // Copiamos la memoria de la variable resetL1 a linea1
  memcpy(linea2, resetL2, 16);
  
  pos=0;
  col=0;
  
  actPantalla();
}
void error() {
  lcd.clear();
  lcd.print("JUEGO FALLIDO");
  
  delay(2000);//Espera un par de segundo
  
  reset();
}
void ganador() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println("Has ganado      ");
  lcd.setCursor(0, 1);
  lcd.println("Pulsa y reinicia");
  delay(2000);
  while(!digitalRead(8)) {
  }
  reset();
  v=v-20;
}
void actPantalla()
{
  //lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println(linea1);
  lcd.setCursor(0, 1);
  lcd.println(linea2);
  Serial.println(linea1);
  delay(v);
}
void loop()
{
  int fail = 0;
  int s=0;
  //actPantalla();
  if(!col)//Si col es igual a 0, la posicion pos de linea1[] sera ' '
    linea1[pos] = VACIO;
  else//Si en cambio es 1, la posicion pos de la linea2 sera ' '
    linea2[pos] = VACIO;
  
  pos++;//pos=pos+1
  if(pos==16)//Si pos es 16, entonces se convieste en 0
    pos=0;
  
  if(digitalRead(8)) {
    //si el pulsador esta pulsado co sera 0
    if (linea2[pos] == FANTASMA)
      fail = 1;
    linea2[pos] = COCO;//Se usan comillas simples poque debe es un char de un caracter y no un string
    col=1;
  } else {
    if (linea1[pos] == FANTASMA)
      fail = 1;
    linea1[pos] = COCO;
    col=0;
  }
  
  for(int n=0; n<16; n++) {
    if(linea1[n]==BOLA) {
      s=1;
      break;
    }
  }
  for(int n=0; n<16; n++) {
    if(linea2[n]==BOLA) {
      s=1;
      break;
    }
  }
  
  actPantalla();
  if (fail)
    error();
  if(!s) {
    ganador();
  }
}

