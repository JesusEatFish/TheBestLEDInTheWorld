
const int buttonPin = 2; //pushButton
const int RledPin = 3; // R
const int GledPin = 5; // G
const int BledPin = 6; // B

int buttonState = 0;
int Mode = 2;
int ledcolor = 0;
bool ButtonPressed = false;

void breath(){
  //Blue
  for (int i = 255; i >= 0; i --){
    analogWrite(BledPin, i);
    delay(10);
  }
  for (int i = 0; i <= 255; i++){
    analogWrite(BledPin, i);
    delay(10);
  }
  digitalWrite(BledPin, HIGH);
  delay(10);
  //Green
  for (int i = 255; i >= 0; i --){
    analogWrite(GledPin, i);
    delay(10);
  }
  for (int i = 0; i <= 255; i++){
    analogWrite(GledPin, i);
    delay(10);
  }
  digitalWrite(GledPin, HIGH);
  delay(10);
  //Red
  for (int i = 255; i >= 0; i --){
    analogWrite(RledPin, i);
    delay(10);
  }
  for (int i = 0; i <= 255; i++){
    analogWrite(RledPin, i);
    delay(10);
  }
  digitalWrite(RledPin, HIGH);
  delay(10);

}

void nightClub(){
  //Blue
  for (int i = 255; i >= 0; i--){
    analogWrite(BledPin, i);
    if (analogRead(BledPin) == 0){
      for (int i = 0; i <= 225; i++){
        analogWrite(BledPin, i);
      }
    }
  }
  digitalWrite(BledPin, HIGH);
  delay(10);
  //Green
  for (int i = 255; i >= 0; i--){
    analogWrite(GledPin, i);
    if (analogRead(GledPin) == 0){
      for (int i = 0; i <= 225; i++){
        analogWrite(GledPin, i);
      }
    }
  }
  digitalWrite(GledPin, HIGH);
  delay(10);
  //Red
  for (int i = 255; i >= 0; i--){
    analogWrite(RledPin, i);
    if (analogRead(RledPin) == 0){
      for (int i = 0; i <= 225; i++){
        analogWrite(RledPin, i);
      }
    }
  }
  digitalWrite(RledPin, HIGH);
  delay(10);

}

void setup() {
  // put your setup code here, to run once:
  pinMode(RledPin, OUTPUT);
  pinMode(GledPin, OUTPUT);
  pinMode(BledPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  digitalWrite(RledPin, HIGH);
  digitalWrite(GledPin, HIGH);
  digitalWrite(BledPin, HIGH);
  Serial.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly
  buttonState = digitalRead(buttonPin);
  if (buttonState == LOW){
    Mode = Mode + 1;
    Serial.print("成功加一");
    Serial.println(Mode);
  }

  /* ==== breath light efect ==== */
  if (Mode % 2 == 0){
    breath();
  }
  /*=============================*/

  /* ======= night club mode ======= */
  if (Mode % 2 == 1){
    nightClub();
  }
  /* ================================= */
}