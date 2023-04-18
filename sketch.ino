/*
Aydın Can Altun - 180202117
Barış Arslan - 180202112
*/
#include<Wire.h>
#include "SevSeg.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
SevSeg sevseg;
#define POTENTIOMETER_PIN A3
#define BUZZER_PIN 53
#define LDR_PIN A0
// LDR Characteristics
const float GAMMA = 0.7;
const float RL10 = 50;
const byte buttonPins[] = {A4, A1, A2}; // 2 -> Sol, 3-> Sağ, 4 -> Aksiyon Al(Seç, Ateş Et)
const byte gunPins[] = {22, 23, 24};
const byte healthPins[] = {38, 39, 40};
const byte digitPins[] = {2, 3, 4, 5};
const byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
byte selectedMenuItem = 0; // 0 -> Kolay, 1 -> Zor
bool isGameStarted = false;
byte remainingGun;
byte playerPosition;
bool isTakeDamage;
bool isFirstAction;
bool isInverted;
/*
Gemi -> >= 6 (6 + Toplam Canı)
Bonus -> == 5
Meteor -> >= 2 (2 + Toplam Canı)
Uzay Çöpü -> == 1
*/
uint8_t spaceMap[8][16] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

uint8_t randomObstaclePosition;
uint8_t randomObstacleType;
uint8_t randomObstacleCount;
uint16_t currentMovementValue;
bool shootButtonClicked;
unsigned long gameTime;
unsigned long damageTakenTime;
uint8_t playerScore;
int ldrValue;
double moveObstacleMiliseconds;
double untouchableMiliseconds;
unsigned long gameStartTime;

void setup() {
  Serial.begin(115200);
  // OLED Başlatılıyor
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  // 7-Segment Display Başlatılıyor
  byte numDigits = 4;
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = true; // Use 'true' if your decimal point doesn't exist or isn't connected
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(90);
  sevseg.setNumber(0, 4);
  // Push Buttonlar INPUT_PULLUP olarak ayarlanıyor.
  for(byte i=0; i<3; i++){
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  // Silah pinleri ayarlanıyor
  for(byte i=0; i<3; i++){
    pinMode(gunPins[i], OUTPUT);
    digitalWrite(gunPins[i], LOW);
  }
  for(byte i=0; i<3; i++){
    pinMode(healthPins[i], OUTPUT);
    digitalWrite(healthPins[i], LOW);
  }
  // Potansiyemetre ayarlanıyor
  pinMode(POTENTIOMETER_PIN, INPUT);
  // Buzzer ayarlanıyor
  pinMode(BUZZER_PIN, OUTPUT);
  // LDR ayarlanıyor
  pinMode(LDR_PIN, INPUT);
  isInverted = false;
  delay(1000);
}


void loop() {
  if(!isGameStarted){
    menu();
    byte clickedButton = takenAction();
    if(!isFirstAction){
      if(clickedButton == A4 || clickedButton == A1){
        selectedMenuItem = selectedMenuItem == 0 ? 1 : 0;
      }else if(clickedButton == A2){
        isGameStarted = true;
      }
    }else{
      isFirstAction = false;
    }
    delay(100);
  }else{
    if(selectedMenuItem == 0){
      Serial.println("Easy Mode");
    }else{
      Serial.println("Hard Mode");
    }
    setGameDefaults();
    while(true){
      sevseg.refreshDisplay();
      if(isGameOver()){
        isGameStarted = false;
        drawGameOver();
        display.display();
        remainingGun = 0;
        showRemainigGun();
        showRemainingHealth();
        byte clickedButton = takenAction();
        isFirstAction = true;
        break;
      }
      if(randomObstacleCount == 14){
        if(remainingGun < 3){
          remainingGun = remainingGun + 1;
        }
        randomObstacleCount = 0;
      }
      if(isTakeDamage){
        if(millis() - damageTakenTime >= untouchableMiliseconds){
          isTakeDamage = false;
        }
      }
      movePlayer();
      showRemainigGun();
      showRemainingHealth();
      if(millis() - gameTime >= moveObstacleMiliseconds){
        moveObstacles();
        createRandomObstacle();
        gameTime = millis();
      }
      drawMap();
      display.display();
      if(digitalRead(A2) == LOW && !isFirstAction){
        shoot();
      }
      if(analogRead(A0) != ldrValue){
        ldrValue = analogRead(A0);
        float voltage = ldrValue / 1024. * 5;
        float resistance = 2000 * voltage / (1-voltage / 5);
        float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
        if(lux > 50){
          display.invertDisplay(false);
        }else{
          display.invertDisplay(true);
        }
      }
      if(selectedMenuItem == 1){
        if(millis() - gameStartTime >= 10000){
          Serial.print("Old Move Obstacle Miliseconds: ");
          Serial.print(moveObstacleMiliseconds);
          moveObstacleMiliseconds = (moveObstacleMiliseconds * 20) / 100;
          Serial.print(", New Move Obstacle Miliseconds: ");
          Serial.print(moveObstacleMiliseconds);
          Serial.print('\n');
          Serial.print("Old Untouchable Miliseconds: ");
          Serial.print(untouchableMiliseconds);
          untouchableMiliseconds = (untouchableMiliseconds * 20) / 100;
          Serial.print(", New Untouchable Miliseconds: ");
          Serial.print(untouchableMiliseconds);
          Serial.print('\n');
          gameStartTime = millis();
        }
      }
      if(isFirstAction){
        isFirstAction = false;
      }
      delay(200);
    }
  }
}
// Menüyü OLED'e Çeker
void menu(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30, 5);
  display.println("Space Shooter");
  if(selectedMenuItem == 0){
    display.fillRect(10, 16, 50, 50, WHITE);
    display.setCursor(23, 38);
    display.setTextColor(BLACK);
    display.setTextSize(1);
    display.println("Easy");
    display.fillRect(70, 16, 50, 50, BLACK);
    display.setCursor(83, 38);
    display.setTextColor(WHITE);
    display.println("Hard");
  }else{
    display.fillRect(10, 16, 50, 50, BLACK);
    display.setCursor(23, 38);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.println("Easy");
    display.fillRect(70, 16, 50, 50, WHITE);
    display.setCursor(83, 38);
    display.setTextColor(BLACK);
    display.println("Hard");
  }
  display.display();
}
// Butondan aksiyon alınmasını bekler. Butondan aksiyon alınca aksiyon alınan butonu döner.
byte takenAction(){
  while(true){
    for(byte i=0; i<3; i++){
      sevseg.refreshDisplay();
      byte buttonPin = buttonPins[i];
      if(digitalRead(buttonPin) == LOW){
        return buttonPin;
      }
    }
    delay(1);
  }
}
// Haritayı çizer
void drawMap(){
  display.clearDisplay();
  for(uint8_t x=0; x<8; x++){
    sevseg.refreshDisplay();
    display.drawLine(0, x*8, 128, x*8, WHITE);
  }
  for(uint8_t y=0; y <16; y++){
    sevseg.refreshDisplay();
    display.drawLine(y*8, 0, y*8, 64, WHITE);
  }
  for(uint8_t i=0; i<8; i++){
    for(uint8_t j=0; j<16; j++){
      sevseg.refreshDisplay();
      if(spaceMap[i][j] > 6){
        drawPlayer(i,j);
      }else if(spaceMap[i][j] == 5){
        drawStar(i, j);
      }else if(spaceMap[i][j] > 2 && spaceMap[i][j] < 5){
        drawMeteor(i, j);
      }else if(spaceMap[i][j] == 1){
        drawSpaceTrash(i, j);
      }
    }
  }
}
// Oyuncu objesini çizer
void drawPlayer(int i, int j){
  int xMin = j*8;
  int yMin = i*8;
  display.fillTriangle(xMin, yMin + 4, xMin + 8, yMin, xMin + 8, yMin + 8, WHITE);
}
// Meteor objesini çizer
void drawMeteor(int i, int j){
  int xMiddle = (j*8) + 4;
  int yMiddle = (i*8) + 4;
  display.fillCircle(xMiddle, yMiddle, 2, WHITE);
}
// Uzay Çöpünü çizer
void drawSpaceTrash(int i, int j){
  int xMin = (j*8);
  int yMin = (i*8);
  display.fillRect(xMin, yMin, 8, 8, WHITE);
}
// Yıldız Çizer
void drawStar(int i, int j){
  int xMin = j*8;
  int yMin = i*8;
  display.fillRect(xMin, yMin, 8, 8, WHITE);
  display.setCursor(xMin + 2, yMin + 1);
  display.setTextColor(BLACK);
  display.println("*");
}
// Varsayılan oyun değerlerini set eder.
void setGameDefaults(){
  for(uint8_t i=0; i<8; i++){
    for(uint8_t j=0; j<16; j++){
      spaceMap[i][j] = 0;
    }
  }
  currentMovementValue = analogRead(A3);
  randomObstacleCount = 0;
  isTakeDamage = false;
  remainingGun = 3;
  playerPosition = 0;
  playerScore = 0;
  spaceMap[0][15] = 9;
  isFirstAction = true;
  sevseg.setNumber(playerScore, 1);
  moveObstacleMiliseconds = 1000;
  untouchableMiliseconds = 3000;
  ldrValue = analogRead(A0);
  gameTime = millis();
  gameStartTime = gameTime;
}
// Rastgele engel/bonus oluşturur.
void createRandomObstacle(){
  bool isFull = false;
  for(uint8_t i=0; i<8; i++){
    sevseg.refreshDisplay();
    if(spaceMap[i][0] == 0){
      isFull = false;
      break;
    }else{
      isFull = true;
    }
  }
  if(!isFull){
    randomObstaclePosition = random(0, 8);
    while(true){
      sevseg.refreshDisplay();
      if(spaceMap[randomObstaclePosition][0] == 0){
        break;
      }else{
        randomObstaclePosition = random(0, 8);
      }
    }
    randomObstacleType = random(0, 3); // 0 -> Çöp, 1 -> Meteor, 2 -> Bonus
    if(randomObstacleType == 0){
      spaceMap[randomObstaclePosition][0] = 1;
      randomObstacleCount = randomObstacleCount + 1;
    }else if(randomObstacleType == 1){
      spaceMap[randomObstaclePosition][0] = 4;
      randomObstacleCount = randomObstacleCount + 1;
    }else if(randomObstacleType == 2){
      spaceMap[randomObstaclePosition][0] = 5;
    }
  }
}
// Haritadaki objeleri ilerletir. Çarpma durumlarını hesaplar.
void moveObstacles(){
  for(uint8_t i=0; i<8; i++){
    for(uint8_t j=15; j>=0 && j<16; j--){
      sevseg.refreshDisplay();
      // Engel ve gemi değil ise
      if(spaceMap[i][j] > 0 && spaceMap[i][j] < 6){
        uint8_t newPosition = j+1;
        // Haritanın Dışına Çıkıyorsa direkt haritadan sil
        if(newPosition > 16){
          spaceMap[i][j] = 0;
        }
        // Haritanın Dışına Çıkmıyor ise
        else{
          // Çarpacağı Obje Gemi mi kontrol et
          if(spaceMap[i][newPosition] > 6){
            if(i == playerPosition && newPosition == 15){
                // Çarpacak obje bonus ise canını arttır
              if(spaceMap[i][j] == 5){
                if(i == playerPosition){
                  spaceMap[playerPosition][15] = spaceMap[playerPosition][15] + 1;
                  spaceMap[i][j] = 0;
                }
              }
              // Değil ise canını azalt ve 3 saniye dokunulmaz yap
              else{
                if(!isTakeDamage){
                  if(i == playerPosition){
                    isTakeDamage = true;
                    damageTakenTime = millis();
                    tone(BUZZER_PIN, 262, 250);
                    if(spaceMap[i][newPosition] - 1 != 6){
                      spaceMap[i][newPosition] = spaceMap[i][newPosition] - 1;
                    }else{
                      spaceMap[i][newPosition] = 6;
                    }
                    spaceMap[i][j] = 0;
                  }
                }else{
                  spaceMap[i][j] = 0;
                }
              }
            }
            // Çarpacak obje bonus ise canını arttır
            if(spaceMap[i][j] == 5){
              spaceMap[i][newPosition] = spaceMap[i][newPosition] + 1;
              spaceMap[i][j] = 0;
            }
            // Değil ise canını azalt ve 3 saniye dokunulmaz yap
            else{
              if(!isTakeDamage){
                isTakeDamage = true;
                damageTakenTime = millis();
                tone(BUZZER_PIN, 262, 250);
                if(spaceMap[i][newPosition] - 1 != 6){
                  spaceMap[i][newPosition] = spaceMap[i][newPosition] - 1;
                }else{
                  spaceMap[i][newPosition] = 6;
                }
                spaceMap[i][j] = 0;
              }else{
                spaceMap[i][j] = 0;
              }
            }
            
          }
          // Bir adım ilerlet
          else{
            uint8_t obstacleValue = spaceMap[i][j];
            spaceMap[i][j] = 0;
            spaceMap[i][newPosition] = obstacleValue;
          }
          
        }
      }
    }
  }
}
// Oyun bitti mi ? Kontrol et
bool isGameOver(){
  return spaceMap[playerPosition][15] == 6;
}

void showRemainigGun(){
  for(uint8_t i=0; i<3; i++){
    sevseg.refreshDisplay();
    if(i<remainingGun){
      digitalWrite(gunPins[i], HIGH);
    }else{
      digitalWrite(gunPins[i], LOW);
    }
  }
}

void showRemainingHealth(){
  uint8_t playerHealth = spaceMap[playerPosition][15] - 6;
  for(uint8_t i=0; i<3; i++){
    sevseg.refreshDisplay();
    if(i < playerHealth){
      digitalWrite(healthPins[i], HIGH);
    }else{
      digitalWrite(healthPins[i], LOW);
    }
  }
}

// Potansiyelmetre 0 - 1023 arasında değer alır. Değeri 512'den büyükse aşağı in değilse yukarı çık.
void movePlayer(){
  uint16_t newValue = analogRead(A3);
  if(newValue != currentMovementValue){
    sevseg.refreshDisplay();
    currentMovementValue = newValue;
    if(newValue > 512){
      // Aşağıya İn
      uint8_t newPosition = playerPosition + 1;
      if(newPosition < 8){
        uint8_t player = spaceMap[playerPosition][15];
        spaceMap[playerPosition][15] = 0;
        spaceMap[newPosition][15] = player;
        playerPosition = newPosition;
        playerScore = playerScore + 1;
        sevseg.setNumber(playerScore, 4);
      }
    }else{
      // Yukarıya Çık
      uint8_t newPosition = playerPosition - 1;
      if(newPosition > -1 && newPosition < 8){
        uint8_t player = spaceMap[playerPosition][15];
        spaceMap[playerPosition][15] = 0;
        spaceMap[newPosition][15] = player;
        playerPosition = newPosition;
        playerScore = playerScore + 1;
        sevseg.setNumber(playerScore, 4);
      }
    }
  }
}
// Ateş et. Objenin canını ayarla veya yoket.
void shoot(){
  if(remainingGun > 0){
    for(uint8_t i = 14; i> -1 && i<15; i--){
      sevseg.refreshDisplay();
        uint8_t target = spaceMap[playerPosition][i];
        if(target > 0){
          if(target == 5){
            spaceMap[playerPosition][i] = 0;
            break;
          }else if(target == 1){
            spaceMap[playerPosition][i] = 0;
            break;
          }else if(target > 2 && target < 5){
            uint8_t newHealth = target - 1;
            spaceMap[playerPosition][i] = newHealth == 2 ? 0 : newHealth;
            break;
          }
        }
      }
    remainingGun = remainingGun - 1;
  }
}
// Oyun sonlanınca skorunu gösterir.
void drawGameOver(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30, 5);
  display.println("Space Shooter");
  display.setCursor(40, 25);
  display.println("Game Over");
  display.setCursor(40, 50);
  display.print("Score:");
  display.print(playerScore);
  display.print("\n");
}