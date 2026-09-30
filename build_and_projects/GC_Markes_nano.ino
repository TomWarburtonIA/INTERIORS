int markes_go_pin = 2;
int GC_go_pin = 3;

void setup() {
  // put your setup code here, to run once:
pinMode(markes_go_pin, INPUT_PULLUP);
pinMode(GC_go_pin, OUTPUT);
digitalWrite(GC_go_pin, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  int go_signal = digitalRead(markes_go_pin);

  if(go_signal == LOW){
    digitalWrite(GC_go_pin, LOW);
  }
  else{
    digitalWrite(GC_go_pin, HIGH);
  }

}
