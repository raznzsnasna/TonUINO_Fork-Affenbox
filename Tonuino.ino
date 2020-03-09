#include <DFMiniMp3.h>
#include <EEPROM.h>
#include <JC_Button.h>
#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <TimerOne.h>
#include <ClickEncoder.h>
/*
   _____         _____ _____ _____ _____
  |_   _|___ ___|  |  |     |   | |     |
    | | | . |   |  |  |-   -| | | |  |  |
    |_| |___|_|_|_____|_____|_|___|_____|
    TonUINO Version 2.1

    created by Thorsten Voß and licensed under GNU/GPL.
    Information and contribution at https://tonuino.de.
*/

///////// uncomment the below line to enable the function ////////////////
//#define FIVEBUTTONS
//#define CHECK_BATTERY
#define DEBUG
#define DEBUG2
//#define PUSH_ON_OFF
//#define STARTUP_SOUND
//#define SPEAKER_SWITCH
//#define ROTARY_ENCODER
//////////////////////////////////////////////////////////////////////////

///////// conifguration of the input and output pin //////////////////////
#define buttonPause A0 //Default A0; Pocket A2
#define buttonUp A1 //Default A1; Pocket A0
#define buttonDown A2 //Default A2; Pocket A1
#define busyPin 4

#define shutdownPin 7 //Default 7

#define openAnalogPin A6 //Default A7

#ifdef FIVEBUTTONS
#define buttonFourPin A3
#define buttonFivePin A4
#endif

#define LONG_PRESS 1000

#ifdef SPEAKER_SWITCH
#define SpeakerOnPin 8
#endif

#ifdef CHECK_BATTERY
#define LEDgreenPin 6
#define LEDredPin 5
#define batMeasPin A7
#endif

#ifdef ROTARY_ENCODER
#define pinA 5 //Default 5; Pocket A0
#define pinB 6 //Default 6; Pocket A1
#define supply 8 //uncomment if you dont want to use an IO pin 
#endif
//////////////////////////////////////////////////////////////////////////

///////// conifguration of the battery measurement ///////////////////////
#ifdef CHECK_BATTERY
#define refVoltage 1.08
#define pinSteps 1024.0

#define batLevel_LEDyellowOn 3.5
#define batLevel_LEDyellowOff 3.7

#define batLevel_LEDredOn 3.1
#define batLevel_LEDredOff 3.2

#define batLevel_Empty 2.9

#define Rone  9920.0 //6680.0
#define Rtwo  990.0 //987.0
#endif
//////////////////////////////////////////////////////////////////////////

///////// conifguration of the rotary encoder ////////////////////////////
#ifdef ROTARY_ENCODER
#define STEPS 4
#endif
//////////////////////////////////////////////////////////////////////////

///////// MFRC522 ////////////////////////////////////////////////////////
#define RST_PIN 9                 // Configurable, see typical pin layout above
#define SS_PIN 10                 // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522
MFRC522::MIFARE_Key key;
bool successRead;
byte sector = 1;
byte blockAddr = 4;
byte trailerBlock = 7;
MFRC522::StatusCode status;
//////////////////////////////////////////////////////////////////////////

///////// setup buttons //////////////////////////////////////////////////
Button pauseButton(buttonPause);
Button upButton(buttonUp);
Button downButton(buttonDown);
#ifdef FIVEBUTTONS
Button buttonFour(buttonFourPin);
Button buttonFive(buttonFivePin);
#endif
bool ignorePauseButton = false;
bool ignoreUpButton = false;
bool ignoreDownButton = false;
#ifdef FIVEBUTTONS
bool ignoreButtonFour = false;
bool ignoreButtonFive = false;
#endif
//////////////////////////////////////////////////////////////////////////

//////// rotary encoder /////////////////////////////////////////////////
#ifdef ROTARY_ENCODER
int16_t oldEncPos = -1 ;
int16_t encPos = 15;
ClickEncoder encoder(pinA, pinB, STEPS);
#endif
//////////////////////////////////////////////////////////////////////////

//////// battery check //////////////////////////////////////////////////
#ifdef CHECK_BATTERY
float Bat_Mean = 0.0;
float Bat_SqrMean = 0.0;
float Bat_VarMean = 0.0;
uint8_t Bat_N = 0;
uint8_t Bat_BatteryLoad = 0;
uint8_t batLevel_EmptyCounter = 0;
uint8_t batLevel_LEDyellowCounter = 0;
uint8_t batLevel_LEDredCounter = 0;
float batLevel_LEDyellow;
float batLevel_LEDred;
#endif 
//////////////////////////////////////////////////////////////////////////

///////// card cookie ////////////////////////////////////////////////////
static const uint32_t cardCookie = 322417479;
//////////////////////////////////////////////////////////////////////////

///////// DFPlayer Mini //////////////////////////////////////////////////
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
uint16_t numTracksInFolder;
uint16_t currentTrack;
uint16_t firstTrack;
uint8_t queue[255];
uint8_t volume;
//////////////////////////////////////////////////////////////////////////

///////// this object stores nfc tag data ///////////////////////////////
struct folderSettings {
  uint8_t folder;
  uint8_t mode;
  uint8_t special;
  uint8_t special2;
};

struct nfcTagObject {
  uint32_t cookie;
  uint8_t version;
  folderSettings nfcFolderSettings;
};
//////////////////////////////////////////////////////////////////////////

///////// admin settings stored in eeprom ///////////////////////////////
struct adminSettings {
  uint32_t cookie;
  byte version;
  uint8_t maxVolume;
  uint8_t minVolume;
  uint8_t initVolume;
  uint8_t eq;
  bool locked;
  long standbyTimer;
  bool invertVolumeButtons;
  folderSettings shortCuts[4];
  uint8_t adminMenuLocked;
  uint8_t adminMenuPin[4];
};

adminSettings mySettings;
nfcTagObject myCard;
folderSettings *myFolder;
unsigned long sleepAtMillis = 0;
static uint16_t _lastTrackFinished;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
#ifdef ROTARY_ENCODER
void timerIsr();
#endif
void shutDown ();
static void nextTrack(uint16_t track);
uint8_t voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
                  bool preview = false, int previewFromFolder = 0, int defaultValue = 0, bool exitWithLongPress = false);
bool isPlaying();
bool checkTwo ( uint8_t a[], uint8_t b[] );
void writeCard(nfcTagObject nfcTag);
void dump_byte_array(byte * buffer, byte bufferSize);
void adminMenu(bool fromCard = false);
bool knownCard = false;
void readButtons();
void playFolder(); 
//////////////////////////////////////////////////////////////////////////

// implement a notification class,
// its member methods will get called
//
class Mp3Notify {
  public:
    static void OnError(uint16_t errorCode) {
      // see DfMp3_Error for code meaning
      Serial.println();
      Serial.print("Com Error ");
      Serial.println(errorCode);
    }
    static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {
      if (source & DfMp3_PlaySources_Sd) Serial.print("SD Karte ");
      if (source & DfMp3_PlaySources_Usb) Serial.print("USB ");
      if (source & DfMp3_PlaySources_Flash) Serial.print("Flash ");
      Serial.println(action);
    }
    static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track) {
      //      Serial.print("Track beendet");
      //      Serial.println(track);
      //      delay(100);
      nextTrack(track);
    }
    static void OnPlaySourceOnline(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "online");
    }
    static void OnPlaySourceInserted(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "bereit");
    }
    static void OnPlaySourceRemoved(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "entfernt");
    }
};

static DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(mySoftwareSerial);
//////////////////////////////////////////////////////////////////////////
void shuffleQueue() {
  // Queue für die Zufallswiedergabe erstellen
  for (uint8_t x = 0; x < numTracksInFolder - firstTrack + 1; x++)
    queue[x] = x + firstTrack;
  // Rest mit 0 auffüllen
  for (uint8_t x = numTracksInFolder - firstTrack + 1; x < 255; x++)
    queue[x] = 0;
  // Queue mischen
  for (uint8_t i = 0; i < numTracksInFolder - firstTrack + 1; i++)
  {
    uint8_t j = random (0, numTracksInFolder - firstTrack + 1);
    uint8_t t = queue[i];
    queue[i] = queue[j];
    queue[j] = t;
  }
#ifdef DEBUG2
  Serial.println(F("Queue :"));
  for (uint8_t x = 0; x < numTracksInFolder - firstTrack + 1 ; x++)
    Serial.println(queue[x]);
#endif
}
//////////////////////////////////////////////////////////////////////////
void writeSettingsToFlash() {
#ifdef DEBUG2
  Serial.println(F("=== writeSettingsToFlash()"));
#endif
  int address = sizeof(myFolder->folder) * 100;
  EEPROM.put(address, mySettings);
}
//////////////////////////////////////////////////////////////////////////
void resetSettings() {
#ifdef DEBUG
  Serial.println(F("=== resetSettings()"));
#endif
  mySettings.cookie = cardCookie;
  mySettings.version = 2;
  mySettings.maxVolume = 25;
  mySettings.minVolume = 5;
  mySettings.initVolume = 15;
  mySettings.eq = 1;
  mySettings.locked = false;
  mySettings.standbyTimer = 0;
  mySettings.invertVolumeButtons = true;
  mySettings.shortCuts[0].folder = 0;
  mySettings.shortCuts[1].folder = 0;
  mySettings.shortCuts[2].folder = 0;
  mySettings.shortCuts[3].folder = 0;
  mySettings.adminMenuLocked = 0;
  mySettings.adminMenuPin[0] = 1;
  mySettings.adminMenuPin[1] = 1;
  mySettings.adminMenuPin[2] = 1;
  mySettings.adminMenuPin[3] = 1;

  writeSettingsToFlash();
}
//////////////////////////////////////////////////////////////////////////
void migrateSettings(int oldVersion) {
  if (oldVersion == 1) {
#ifdef DEBUG
    Serial.println(F("=== resetSettings()"));
    Serial.println(F("1 -> 2"));
#endif
    mySettings.version = 2;
    mySettings.adminMenuLocked = 0;
    mySettings.adminMenuPin[0] = 1;
    mySettings.adminMenuPin[1] = 1;
    mySettings.adminMenuPin[2] = 1;
    mySettings.adminMenuPin[3] = 1;
    writeSettingsToFlash();
  }
}
//////////////////////////////////////////////////////////////////////////
void loadSettingsFromFlash() {
#ifdef DEBUG
  Serial.println(F("=== loadSettingsFromFlash()"));
#endif
  int address = sizeof(myFolder->folder) * 100;
  EEPROM.get(address, mySettings);
  if (mySettings.cookie != cardCookie)
    resetSettings();
  migrateSettings(mySettings.version);

#ifdef DEBUG
  Serial.print(F("Version: "));
  Serial.println(mySettings.version);

  Serial.print(F("Maximal Volume: "));
  Serial.println(mySettings.maxVolume);

  Serial.print(F("Minimal Volume: "));
  Serial.println(mySettings.minVolume);

  Serial.print(F("Initial Volume: "));
  Serial.println(mySettings.initVolume);

  Serial.print(F("EQ: "));
  Serial.println(mySettings.eq);

  Serial.print(F("Locked: "));
  Serial.println(mySettings.locked);

  Serial.print(F("Sleep Timer: "));
  Serial.println(mySettings.standbyTimer);

  Serial.print(F("Inverted Volume Buttons: "));
  Serial.println(mySettings.invertVolumeButtons);

  Serial.print(F("Admin Menu locked: "));
  Serial.println(mySettings.adminMenuLocked);

  Serial.print(F("Admin Menu Pin: "));
  Serial.print(mySettings.adminMenuPin[0]);
  Serial.print(mySettings.adminMenuPin[1]);
  Serial.print(mySettings.adminMenuPin[2]);
  Serial.println(mySettings.adminMenuPin[3]);
#endif
}

/// Funktionen für den Standby Timer (z.B. über Pololu-Switch oder Mosfet)

void setstandbyTimer() {

#ifdef DEBUG
  Serial.println(F("=== setstandbyTimer()"));
#endif
  if (mySettings.standbyTimer != 0)
    sleepAtMillis = millis() + (mySettings.standbyTimer * 60 * 1000);
  else
    sleepAtMillis = 0;
#ifdef DEBUG2
  Serial.println(sleepAtMillis);
#endif
}
//////////////////////////////////////////////////////////////////////////
void disablestandbyTimer() {
#ifdef DEBUG
  Serial.println(F("=== disablestandby()"));
#endif
  sleepAtMillis = 0;
}
//////////////////////////////////////////////////////////////////////////
void checkStandbyAtMillis() {
  if (sleepAtMillis != 0 && millis() > sleepAtMillis) {
#ifdef DEBUG
    Serial.println(F("=== power off!"));
#endif
    // enter sleep state
    digitalWrite(shutdownPin, HIGH);
    delay(500);

    // http://discourse.voss.earth/t/intenso-s10000-powerbank-automatische-abschaltung-software-only/805
    // powerdown to 27mA (powerbank switches off after 30-60s)
    mfrc522.PCD_AntennaOff();
    mfrc522.PCD_SoftPowerDown();
    mp3.sleep();

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();  // Disable interrupts
    sleep_mode();
  }
}
//////////////////////////////////////////////////////////////////////////
class Modifier {
  public:
    virtual void loop() {}
    virtual bool handlePause() {
      return false;
    }
    virtual bool handleNext() {
      return false;
    }
    virtual bool handlePrevious() {
      return false;
    }
    virtual bool handleNextButton() {
      return false;
    }
    virtual bool handlePreviousButton() {
      return false;
    }
    virtual bool handleVolumeUp() {
      return false;
    }
    virtual bool handleVolumeDown() {
      return false;
    }
    virtual bool handleShutDown() {
      return false;
    }
    virtual bool handleRFID(nfcTagObject *newCard) {
      return false;
    }
    virtual uint8_t getActive() {
      return 0;
    }
    Modifier() {

    }
};

Modifier *activeModifier = NULL;
//////////////////////////////////////////////////////////////////////////
class SleepTimer: public Modifier {
  private:
    unsigned long sleepAtMillis = 0;

  public:
    void loop() {
      if (this->sleepAtMillis != 0 && millis() > this->sleepAtMillis) {
#ifdef DEBUG2
        Serial.println(F("=== SleepTimer::loop() -> SLEEP!"));
#endif
        mp3.pause();
        setstandbyTimer();
        activeModifier = NULL;
        delete this;
      }
    }

    SleepTimer(uint8_t minutes) {
#ifdef DEBUG
      Serial.println(F("=== SleepTimer()"));
      Serial.println(minutes);
#endif
      this->sleepAtMillis = millis() + minutes * 60000;
      //      if (isPlaying())
      //        mp3.playAdvertisement(302);
      //      delay(500);
    }
    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== SleepTimer::getActive()"));
#endif
      return 1;
    }
};
//////////////////////////////////////////////////////////////////////////
class FreezeDance: public Modifier {
  private:
    unsigned long nextStopAtMillis = 0;
    const uint8_t minSecondsBetweenStops = 5;
    const uint8_t maxSecondsBetweenStops = 30;

    void setNextStopAtMillis() {
      uint16_t seconds = random(this->minSecondsBetweenStops, this->maxSecondsBetweenStops + 1);
#ifdef DEBUG2
      Serial.println(F("=== FreezeDance::setNextStopAtMillis()"));
      Serial.println(seconds);
#endif
      this->nextStopAtMillis = millis() + seconds * 1000;
    }

  public:
    void loop() {
      if (this->nextStopAtMillis != 0 && millis() > this->nextStopAtMillis) {
#ifdef DEBUG2
        Serial.println(F("== FreezeDance::loop() -> FREEZE!"));
#endif
        if (isPlaying()) {
          mp3.playAdvertisement(301);
          delay(500);
        }
        setNextStopAtMillis();
      }
    }
    FreezeDance(void) {
#ifdef DEBUG
      Serial.println(F("=== FreezeDance()"));
#endif
      if (isPlaying()) {
        delay(1000);
        mp3.playAdvertisement(300);
        delay(500);
      }
      setNextStopAtMillis();
    }
    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== FreezeDance::getActive()"));
#endif
      return 2;
    }
};
//////////////////////////////////////////////////////////////////////////
class PuzzleGame: public Modifier {
  private:
    uint8_t PartOneSpecial = 0;
    uint8_t PartOneFolder = 0;
    uint8_t PartOneSpecial2 = 0;

    uint8_t PartTwoSpecial = 0;
    uint8_t PartTwoFolder = 0;
    uint8_t PartTwoSpecial2 = 0;

    uint8_t mode = 0;

    bool PartOneSaved = false;
    bool PartTwoSaved = false;

    nfcTagObject tmpCard;

    void Success ()
    {
      PartOneSaved = false;
      PartTwoSaved = false;
#ifdef DEBUG
      Serial.println(F("== PuzzleGame::Success()"));
#endif

      mp3.playMp3FolderTrack(297); //Toll gemacht! Das ist richtig.
      waitForTrackToFinish();
    }

    void Failure ()
    {
      if (mode == 1) {
        PartOneSaved = false;
      }
      PartTwoSaved = false;

#ifdef DEBUG
      Serial.println(F("== PuzzleGame::Failure()"));
#endif

      mp3.playMp3FolderTrack(296); //Bist du dir sicher das diese Karte richtig ist? Versuche es doch noch einmal.
      waitForTrackToFinish();
    }


    void CompareParts()
    {
      if ((PartOneSpecial == PartTwoSpecial) && (PartOneFolder == PartTwoFolder) && (PartOneSpecial2 == PartTwoSpecial2)) {
        PartTwoSaved = false;
        return;
      }
      else if (((PartOneSpecial !=  PartTwoSpecial) || (PartOneFolder != PartTwoFolder)) && (PartOneSpecial2 == PartTwoSpecial2)) {
        Success();
      }
      else {
        Failure();
      }

    }

  public:
    void loop()
    {
      if (PartOneSaved && PartTwoSaved) {
        if (!isPlaying()) {
          CompareParts();
        }
      }
      if (upButton.pressedFor(LONG_PRESS) && downButton.pressedFor(LONG_PRESS)) {
        do {
          readButtons();
        } while (upButton.isPressed() || downButton.isPressed());
#ifdef DEBUG2
        Serial.println(F("== PuzzleGame::loop() -> Delete current part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(298); //Okay, lass uns was anderes probieren.
        PartOneSaved = false;
        PartTwoSaved = false;
      }
    }

    PuzzleGame(uint8_t special)
    {
      mp3.loop();
      mp3.pause();
#ifdef DEBUG
      Serial.println(F("=== PuzzleGame()"));
#endif

      mode = special;
    }

    virtual bool handlePause() {
      return false;
    }

    virtual bool handleNext() {
#ifdef DEBUG2
      Serial.println(F("== PuzzleGame::handleNext() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== PuzzleGame::handleNextButton() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== PuzzleGame::handlePreviousButton() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handleRFID(nfcTagObject *newCard) {

      this->tmpCard = *newCard;
      if (tmpCard.nfcFolderSettings.mode != 10) {
#ifdef DEBUG2
        Serial.println(F("== PuzzleGame::handleRFID() -> No Valid Puzzle Part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(299); //Diese Karte ist nicht Teil des Spiels. Probiere eine Andere.
        return true;
      }
      else {
#ifdef DEBUG2
        Serial.println(F("== PuzzleGame::handleRFID() -> Valid Puzzle Part"));
#endif
        if (!PartOneSaved)
        {
          PartOneSpecial = tmpCard.nfcFolderSettings.special;
          PartOneFolder = tmpCard.nfcFolderSettings.folder;
          PartOneSpecial2 = tmpCard.nfcFolderSettings.special2;
          PartOneSaved = true;
        }
        else if (PartOneSaved && !PartTwoSaved) {
          PartTwoSpecial = tmpCard.nfcFolderSettings.special;
          PartTwoFolder = tmpCard.nfcFolderSettings.folder;
          PartTwoSpecial2 = tmpCard.nfcFolderSettings.special2;
          PartTwoSaved = true;
        }
        return false;
      }
    }

    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== PuzzleGame::getActive()"));
#endif
      return 8;
    }
};
//////////////////////////////////////////////////////////////////////////
class QuizGame: public Modifier {
  private:
    uint8_t PartOneSpecial = 0;
    uint8_t PartOneFolder = 0;
    uint8_t PartOneSpecial2 = 0;

    uint8_t PartTwoSpecial = 0;
    uint8_t PartTwoFolder = 0;
    uint8_t PartTwoSpecial2 = 0;

    bool PartOneSaved = false;
    bool PartTwoSaved = false;

    nfcTagObject tmpCard;

    void Success () {
      PartOneSaved = false;
      PartTwoSaved = false;
#ifdef DEBUG
      Serial.println(F("== QuizGame::Success()"));
#endif
      mp3.playMp3FolderTrack(297);
      waitForTrackToFinish();
      next();
    }

    void Failure () {
      PartTwoSaved = false;
#ifdef DEBUG
      Serial.println(F("== QuizGame::Failure()"));
#endif
      mp3.playMp3FolderTrack(296);
      waitForTrackToFinish();
    }

    void CompareParts() {
      if ((PartOneSpecial == PartTwoSpecial) && (PartOneFolder == PartTwoFolder) && (PartOneSpecial2 == PartTwoSpecial2)) {
        PartTwoSaved = false;
        return;
      }
      else if (((PartOneSpecial !=  PartTwoSpecial) || (PartOneFolder != PartTwoFolder)) && (PartOneSpecial2 == PartTwoSpecial2)) {
        Success();
      }
      else {
        Failure();
      }
    }

  public:
    void loop() {

      if (PartOneSaved && PartTwoSaved) {
        if (!isPlaying()) {
#ifdef DEBUG2
          Serial.println(F("== QuizGame::loop() -> Compare"));
#endif
          CompareParts();
        }
      }

      if (upButton.pressedFor(LONG_PRESS) && downButton.pressedFor(LONG_PRESS)) {
        do {
          readButtons();
        } while (upButton.isPressed() || downButton.isPressed());
#ifdef DEBUG2
        Serial.println(F("== QuizGame::loop() -> Delete current part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(298); //Okay, lass uns was anderes probieren.
        waitForTrackToFinish();
        PartOneSaved = false;
        PartTwoSaved = false;
        next();
      }
    }

    QuizGame(uint8_t special) {
      mp3.loop();
#ifdef DEBUG
      Serial.println(F("=== QuizGame()"));
#endif
      mp3.pause();


      PartOneFolder = special;
      numTracksInFolder = getFolderMP3Count(PartOneFolder);
      firstTrack = 1;
      shuffleQueue();
      currentTrack = 0;

#ifdef DEBUG2
      Serial.println(F("=== QuizGame() -> Queue generated"));
#endif
      next();
    }

    void  next() {
      numTracksInFolder = getFolderMP3Count(PartOneFolder);
#ifdef DEBUG2
      Serial.println(F("== QuizGame::next()"));
      Serial.println(currentTrack);
      Serial.println(numTracksInFolder - firstTrack + 1);
#endif
      if (currentTrack != numTracksInFolder - firstTrack + 1) {
#ifdef DEBUG2
        Serial.println(F("== QuizGame::next()-> weiter in der Queue "));
#endif
        currentTrack++;
      } else {
#ifdef DEBUG2
        Serial.println(F("== QuizGame::next() -> beginne von vorne"));
#endif
        currentTrack = 1;
      }
      PartOneSaved = true;
      PartOneSpecial = queue[currentTrack - 1];
      PartOneSpecial2 = PartOneSpecial;
#ifdef DEBUG
      Serial.println(currentTrack);
      Serial.println(PartOneSpecial);
#endif
      PlayFolderMP3(PartOneFolder, PartOneSpecial);
    }

    virtual bool handleNext() {
#ifdef DEBUG2
      Serial.println(F("== QuizGame::handleNext() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handlePause() {
#ifdef DEBUG2
      Serial.println(F("== QuizGame::handlePause() -> Pause current Track and repeat question"));
#endif
      mp3.pause();
      delay(100);
      PlayFolderMP3(PartOneFolder, PartOneSpecial);
      return true;
    }

    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== QuizGame::handleNextButton() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== QuizGame::handlePreviousButton() -> LOCKED!"));
#endif
      return true;
    }

    virtual bool handleRFID(nfcTagObject * newCard) {

      this->tmpCard = *newCard;
      if (tmpCard.nfcFolderSettings.mode != 10) {
#ifdef DEBUG2
        Serial.println(F("== QuizGame::handleRFID() -> No Valid Puzzle Part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(299); //Diese Karte ist nicht Teil des Spiels. Probiere eine Andere.
        return true;
      }
      else {
#ifdef DEBUG2
        Serial.println(F("== QuizGame::handleRFID() -> Valid Puzzle Part"));
#endif
        if (PartOneSaved && !PartTwoSaved) {
          PartTwoSpecial = tmpCard.nfcFolderSettings.special;
          PartTwoFolder = tmpCard.nfcFolderSettings.folder;
          PartTwoSpecial2 = tmpCard.nfcFolderSettings.special2;
          PartTwoSaved = true;
        }
        return false;
      }
    }

    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== QuizGame::getActive()"));
#endif
      return 9;
    }
};
//////////////////////////////////////////////////////////////////////////
class ButtonSmash: public Modifier {
  private:
    uint8_t Folder;

    nfcTagObject tmpCard;

  public:
    void loop() {
    }

    ButtonSmash(uint8_t special, uint8_t special2) {
      mp3.loop();
#ifdef DEBUG
      Serial.println(F("=== ButtonSmash()"));
#endif
      mp3.pause();
      delay(200);
#ifdef DEBUG2
      Serial.println(F("=== ButtonSmash() -> Set Volume"));
#endif
      mp3.setVolume(special2);
      delay(200);
#ifdef DEBUG2
      Serial.println(F("=== ButtonSmash() -> Set Folder"));
#endif
      Folder = special;
      numTracksInFolder = getFolderMP3Count(Folder);
#ifdef DEBUG2
      Serial.println(F("=== ButtonSmash() -> generate queue"));
#endif
      firstTrack = 1;
      shuffleQueue();
      currentTrack = 0;

#ifdef DEBUG2
      Serial.println(F("=== ButtonSmash() -> Init End"));
#endif
    }

    void  next() {
      mp3.pause();
      delay(100);
      numTracksInFolder = getFolderMP3Count(Folder);
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::next()"));
      Serial.println(currentTrack);
      Serial.println(numTracksInFolder - firstTrack + 1);
#endif
      if (currentTrack != numTracksInFolder - firstTrack + 1) {
#ifdef DEBUG2
        Serial.println(F("== ButtonSmash::next()-> weiter in der Queue "));
#endif
        currentTrack++;
      } else {
#ifdef DEBUG2
        Serial.println(F("== ButtonSmash::next() -> beginne von vorne"));
#endif
        firstTrack = 1;
        shuffleQueue();
        currentTrack = 1;
      }
      PlayFolderMP3(Folder, queue[currentTrack - 1]);
      waitForTrackToFinish();
    }

    virtual bool handleNext() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handleNext()"));
#endif
      return true;
    }

    virtual bool handlePause() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmashame::handlePause()"));
#endif
      next();
      return true;
    }

    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handleNextButton()"));
#endif
      next();
      return true;
    }

    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handlePreviousButton()"));
#endif
      next();
      return true;
    }

    virtual bool handleVolumeUp() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handlePreviousButton()"));
#endif
      next();
      return true;
    }
    virtual bool handleVolumeDown() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handlePreviousButton()"));
#endif
      next();
      return true;
    }

    virtual bool handleShutDown() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handleShutDown()"));
#endif
      next();
      return true;
    }
    virtual bool handleRFID(nfcTagObject * newCard) {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::handleRFID() -> No Valid Puzzle Part"));
#endif
      next();
      return true;
    }

    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== ButtonSmash::getActive()"));
#endif
      return 10;
    }
};
//////////////////////////////////////////////////////////////////////////
class Locked: public Modifier {
  public:
    virtual bool handlePause()     {
#ifdef DEBUG2
      Serial.println(F("== Locked::handlePause() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleNextButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== Locked::handlePreviousButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleVolumeUp()   {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleVolumeUp() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleVolumeDown() {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleVolumeDown() -> LOCKED!"));
#endif
      return true;
    }
     virtual bool handleShutDown() {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleShutDown() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleRFID(nfcTagObject *newCard) {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleRFID() -> LOCKED!"));
#endif
      return true;
    }
    Locked(void) {
#ifdef DEBUG
      Serial.println(F("=== Locked()"));
#endif
    }
    uint8_t getActive() {
      return 3;
    }
};
//////////////////////////////////////////////////////////////////////////
class ToddlerMode: public Modifier {
  public:
    virtual bool handlePause()     {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::handlePause() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::handleNextButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::handlePreviousButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleVolumeUp()   {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::handleVolumeUp() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleVolumeDown() {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::handleVolumeDown() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleShutDown() {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleShutDown() -> LOCKED!"));
#endif
      return true;
    }
    ToddlerMode(void) {
#ifdef DEBUG
      Serial.println(F("=== ToddlerMode()"));
#endif
    }
    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== ToddlerMode::getActive()"));
#endif
      return 4;
    }
};
//////////////////////////////////////////////////////////////////////////
class KindergardenMode: public Modifier {
  private:
    nfcTagObject nextCard;
    bool cardQueued = false;

  public:
    virtual bool handleNext() {
#ifdef DEBUG2
      Serial.println(F("== KindergardenMode::handleNext() -> NEXT"));
#endif
      if (this->cardQueued == true) {
        this->cardQueued = false;

        myCard = nextCard;
        if (&myCard.nfcFolderSettings > 255)
          myFolder = &myCard.nfcFolderSettings - 255;
        else
          myFolder = &myCard.nfcFolderSettings;
#ifdef DEBUG2
        Serial.println(myFolder->folder);
        Serial.println(myFolder->mode);
#endif
        playFolder();
        return true;
      }
      return false;
    }
    virtual bool handleNextButton()       {
#ifdef DEBUG2
      Serial.println(F("== KindergardenMode::handleNextButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handlePreviousButton() {
#ifdef DEBUG2
      Serial.println(F("== KindergardenMode::handlePreviousButton() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleShutDown() {
#ifdef DEBUG2
      Serial.println(F("== Locked::handleShutDown() -> LOCKED!"));
#endif
      return true;
    }
    virtual bool handleRFID(nfcTagObject * newCard) { // lot of work to do!
#ifdef DEBUG2
      Serial.println(F("== KindergardenMode::handleRFID() -> queued!"));
#endif
      this->nextCard = *newCard;
      this->cardQueued = true;
      if (!isPlaying()) {
        handleNext();
      }
      return true;
    }
    KindergardenMode() {
#ifdef DEBUG
      Serial.println(F("=== KindergardenMode()"));
#endif
    }
    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== KindergardenMode::getActive()"));
#endif
      return 5;
    }
};
//////////////////////////////////////////////////////////////////////////
class RepeatSingleModifier: public Modifier {
  public:
    virtual bool handleNext() {
#ifdef DEBUG2
      Serial.println(F("== RepeatSingleModifier::handleNext() -> REPEAT CURRENT TRACK"));
#endif
      delay(50);
      if (isPlaying()) return true;
      PlayFolderMP3(myFolder->folder, currentTrack);        
      _lastTrackFinished = 0;
      return true;
    }
    RepeatSingleModifier() {
#ifdef DEBUG
      Serial.println(F("=== RepeatSingleModifier()"));
#endif
    }
    uint8_t getActive() {
#ifdef DEBUG2
      Serial.println(F("== RepeatSingleModifier::getActive()"));
#endif
      return 6;
    }
};
//////////////////////////////////////////////////////////////////////////
// An modifier can also do somethings in addition to the modified action
// by returning false (not handled) at the end
// This simple FeedbackModifier will tell the volume before changing it and
// give some feedback once a RFID card is detected.
class FeedbackModifier: public Modifier {
  public:
    virtual bool handleVolumeDown() {
      if (volume > mySettings.minVolume) {
        mp3.playAdvertisement(volume - 1);
      }
      else {
        mp3.playAdvertisement(volume);
      }
      delay(500);
#ifdef DEBUG2
      Serial.println(F("== FeedbackModifier::handleVolumeDown()!"));
#endif
      return false;
    }
    virtual bool handleVolumeUp() {
      if (volume < mySettings.maxVolume) {
        mp3.playAdvertisement(volume + 1);
      }
      else {
        mp3.playAdvertisement(volume);
      }
      delay(500);
#ifdef DEBUG2
      Serial.println(F("== FeedbackModifier::handleVolumeUp()!"));
#endif
      return false;
    }
    virtual bool handleRFID(nfcTagObject *newCard) {
#ifdef DEBUG2
      Serial.println(F("== FeedbackModifier::handleRFID()"));
#endif
      return false;
    }
};
//////////////////////////////////////////////////////////////////////////
// Leider kann das Modul selbst keine Queue abspielen, daher müssen wir selbst die Queue verwalten
static void nextTrack(uint16_t track) {
#ifdef DEBUG
  Serial.print("== nextTrack: Track No.");
  Serial.println(track);
#endif

  if (activeModifier != NULL)
    if (activeModifier->handleNext() == true)
      return;

  if (track == _lastTrackFinished) {
    return;
  }
  _lastTrackFinished = track;

  if (knownCard == false)
    // Wenn eine neue Karte angelernt wird soll das Ende eines Tracks nicht
    // verarbeitet werden
    return;

#ifdef DEBUG
  Serial.println(F("=== nextTrack()"));
#endif

  if (myFolder->mode == 1 || myFolder->mode == 7) {
#ifdef DEBUG2
    Serial.println(F("Hörspielmodus ist aktiv -> keinen neuen Track spielen"));
#endif
    setstandbyTimer();
    
  }
  if (myFolder->mode == 2 || myFolder->mode == 8) {
    if (currentTrack != numTracksInFolder) {
      currentTrack = currentTrack + 1;
      PlayFolderMP3(myFolder->folder, currentTrack);
#ifdef DEBUG2
      Serial.print(F("Albummodus ist aktiv -> nächster Track: "));
      Serial.print(currentTrack);
#endif
    } else
     
      setstandbyTimer();
    { }
  }
  if (myFolder->mode == 3 || myFolder->mode == 9 || myFolder->mode == 10) {
    if (currentTrack != numTracksInFolder - firstTrack + 1) {
#ifdef DEBUG2
      Serial.print(F("Party -> weiter in der Queue "));
#endif
      currentTrack++;
    } else {
#ifdef DEBUG2
      Serial.println(F("Ende der Queue -> beginne von vorne"));
#endif
      currentTrack = 1;
      //// Wenn am Ende der Queue neu gemischt werden soll bitte die Zeilen wieder aktivieren
      //     Serial.println(F("Ende der Queue -> mische neu"));
      //     shuffleQueue();
    }
#ifdef DEBUG2
    Serial.println(queue[currentTrack - 1]);
#endif
    PlayFolderMP3(myFolder->folder, queue[currentTrack - 1]);
  }

  if (myFolder->mode == 4) {
#ifdef DEBUG2
    Serial.println(F("Einzel Modus aktiv"));
#endif
   
    setstandbyTimer();
  }
  if (myFolder->mode == 5 || myFolder->mode == 11) {
    if (currentTrack + 1 < firstTrack + numTracksInFolder) {
      currentTrack = currentTrack + 1;
#ifdef DEBUG2
      Serial.print(F("Hörbuch Modus ist aktiv -> nächster Track und "
                     "Fortschritt speichern"));
      Serial.println(currentTrack);
#endif

       PlayFolderMP3(myFolder->folder, currentTrack);
        
      // Fortschritt im EEPROM abspeichern
      EEPROM.update(myFolder->folder, currentTrack);
    } else {
      // Fortschritt zurücksetzen (wenn vorhanden: auf das nächste Hörspiel im Ordner. sonst: ganz auf den Anfang)
    if(currentTrack < getFolderMP3Count(myFolder->folder)) {
      EEPROM.update(myFolder->folder, firstTrack + numTracksInFolder);
    } else {
      EEPROM.update(myFolder->folder, 1);
    }
      setstandbyTimer();
    }
  }
  delay(500);
}
//////////////////////////////////////////////////////////////////////////
static void previousTrack() {
#ifdef DEBUG
  Serial.println(F("=== previousTrack()"));
#endif
  if (myFolder->mode == 2 || myFolder->mode == 8) {
#ifdef DEBUG2
    Serial.println(F("Albummodus ist aktiv -> vorheriger Track"));
#endif
    if (currentTrack != firstTrack) {
      currentTrack = currentTrack - 1;
    }
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  if (myFolder->mode == 3 || myFolder->mode == 9) {
    if (currentTrack != 1) {
#ifdef DEBUG2
      Serial.print(F("Party Modus ist aktiv -> zurück in der Qeueue "));
#endif
      currentTrack--;
    }
    else
    {
#ifdef DEBUG2
      Serial.print(F("Anfang der Queue -> springe ans Ende "));
#endif
      currentTrack = numTracksInFolder;
    }
#ifdef DEBUG2
    Serial.println(queue[currentTrack - 1]);
#endif
    PlayFolderMP3(myFolder->folder, queue[currentTrack - 1]);
  }
  if (myFolder->mode == 4) {
#ifdef DEBUG2
    Serial.println(F("Einzel Modus aktiv -> Track von vorne spielen"));
#endif
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  if (myFolder->mode == 5 || myFolder->mode == 11) {
#ifdef DEBUG 2
    Serial.println(F("Hörbuch Modus ist aktiv -> vorheriger Track und Fortschritt speichern"));
#endif
    if (currentTrack > firstTrack) {
      currentTrack = currentTrack - 1;
    }
    PlayFolderMP3(myFolder->folder, currentTrack);
    // Fortschritt im EEPROM abspeichern
    EEPROM.update(myFolder->folder, currentTrack);
  }
  delay(1000);
}
//////////////////////////////////////////////////////////////////////////
bool isPlaying() {
  return !digitalRead(busyPin);
}
//////////////////////////////////////////////////////////////////////////
void waitForTrackToFinish() {
  long currentTime = millis();
#define TIMEOUT 1000
  do {
    mp3.loop();
  } while (!isPlaying() && millis() < currentTime + TIMEOUT);
  delay(100);
  do {
    mp3.loop();
  } while (isPlaying());
}
//////////////////////////////////////////////////////////////////////////
void setColor(int redValue, int greenValue, int blueValue) {
#ifdef CHECK_BATTERY
  analogWrite(LEDredPin, redValue);
  analogWrite(LEDgreenPin, greenValue);
  //analogWrite(LEDbluePin, blueValue);
#endif
}
//////////////////////////////////////////////////////////////////////////
void setup() {
#ifdef DEBUG
  Serial.begin(115200); // Es gibt ein paar Debug Ausgaben über die serielle Schnittstelle
#endif
  // Busy Pin
  pinMode(busyPin, INPUT);
  mp3.begin();
  delay(2000);
  mp3.loop();
  
  // Wert für randomSeed() erzeugen durch das mehrfache Sammeln von rauschenden LSBs eines offenen Analogeingangs
  uint32_t ADC_LSB;
  uint32_t ADCSeed;
  for (uint8_t i = 0; i < 128; i++) {
    ADC_LSB = analogRead(openAnalogPin) & 0x1;
    ADCSeed ^= ADC_LSB << (i % 32);
  }
  randomSeed(ADCSeed); // Zufallsgenerator initialisieren
#ifdef DEBUG
  // Dieser Hinweis darf nicht entfernt werden
  Serial.println(F("\n _____         _____ _____ _____ _____"));
  Serial.println(F("|_   _|___ ___|  |  |     |   | |     |"));
  Serial.println(F("  | | | . |   |  |  |-   -| | | |  |  |"));
  Serial.println(F("  |_| |___|_|_|_____|_____|_|___|_____|\n"));
  Serial.println(F("TonUINO Version 2.1"));
  Serial.println(F("created by Thorsten Voß and licensed under GNU/GPL."));
  Serial.println(F("Information and contribution at https://tonuino.de.\n"));
#endif

#ifdef ROTARY_ENCODER  
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder.setAccelerationEnabled(false);
  #ifdef supply
    pinMode(supply, OUTPUT);
    digitalWrite(supply, HIGH);
  #endif
#endif

#ifdef SPEAKER_SWITCH
  pinMode(SpeakerOnPin, OUTPUT);
  digitalWrite(SpeakerOnPin, LOW);
#endif
#ifdef CHECK_BATTERY
  analogReference(INTERNAL);
  pinMode(LEDredPin, OUTPUT);
  pinMode(LEDgreenPin, OUTPUT);

  setColor(0, 10, 0); // White Color

  batLevel_LEDyellow = batLevel_LEDyellowOn;
  batLevel_LEDred = batLevel_LEDredOn;
#endif

  // load Settings from EEPROM
  loadSettingsFromFlash();

  // activate standby timer
  setstandbyTimer();

  // DFPlayer Mini initialisieren
  //  mp3.begin();
  //  // Zwei Sekunden warten bis der DFPlayer Mini initialisiert ist
  //  delay(2000);
  volume = mySettings.initVolume;
  mp3.setVolume(volume);
  mp3.setEq(mySettings.eq - 1);

  // NFC Leser initialisieren
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
#ifdef DEBUG2
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader
#endif
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  pinMode(buttonPause, INPUT_PULLUP);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
#ifdef FIVEBUTTONS
  pinMode(buttonFourPin, INPUT_PULLUP);
  pinMode(buttonFivePin, INPUT_PULLUP);
#endif
  pinMode(shutdownPin, OUTPUT);
  digitalWrite(shutdownPin, LOW);


  // RESET --- ALLE DREI KNÖPFE BEIM STARTEN GEDRÜCKT HALTEN -> alle EINSTELLUNGEN werden gelöscht
  if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW &&
      digitalRead(buttonDown) == LOW) {
#ifdef DEBUG
    Serial.println(F("Reset -> EEPROM wird gelöscht"));
#endif
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.update(i, 0);
    }
    loadSettingsFromFlash();
  }
#ifdef SPEAKER_SWITCH
  digitalWrite(SpeakerOnPin, HIGH);
  delay(100);
#endif

  // Start Shortcut "at Startup" - e.g. Welcome Sound
  //playShortCut(3);

#ifdef STARTUP_SOUND
  mp3.playMp3FolderTrack(264);
  delay(500);
  waitForTrackToFinish();
#endif
}
//////////////////////////////////////////////////////////////////////////
void readButtons() {
  pauseButton.read();
  upButton.read();
  downButton.read();
#ifdef FIVEBUTTONS
  buttonFour.read();
  buttonFive.read();
#endif
}
//////////////////////////////////////////////////////////////////////////
#ifndef ROTARY_ENCODER
void volumeUpButton() {
  if (activeModifier != NULL)
    if (activeModifier->handleVolumeUp() == true)
      return;
#ifdef DEBUG
  Serial.println(F("=== volumeUp()"));
#endif
  if (volume < mySettings.maxVolume) {
    mp3.increaseVolume();
    volume++;
  }
#ifdef DEBUG
  Serial.println(volume);
#endif
}
//////////////////////////////////////////////////////////////////////////
void volumeDownButton() {
  if (activeModifier != NULL)
    if (activeModifier->handleVolumeDown() == true)
      return;

#ifdef DEBUG
  Serial.println(F("=== volumeDown()"));
#endif
  if (volume > mySettings.minVolume) {
    mp3.decreaseVolume();
    volume--;
  }
#ifdef DEBUG
  Serial.println(volume);
#endif
}
#endif
//////////////////////////////////////////////////////////////////////////
void nextButton() {


  if (activeModifier != NULL)
    if (activeModifier->handleNextButton() == true)
      return;

  nextTrack(random(65536));
  delay(300);
}
//////////////////////////////////////////////////////////////////////////
void previousButton() {


  if (activeModifier != NULL)
    if (activeModifier->handlePreviousButton() == true)
      return;

  previousTrack();
  delay(300);
}
//////////////////////////////////////////////////////////////////////////
void playFolder() {
  uint8_t counter = 0;
#ifdef DEBUG
  Serial.println(F("== playFolder()")) ;
#endif
  mp3.loop();
  disablestandbyTimer();
  knownCard = true;
  _lastTrackFinished = 0;
  numTracksInFolder = getFolderMP3Count(myFolder->folder);
  firstTrack = 1;
#ifdef DEBUG2
  Serial.print(numTracksInFolder);
  Serial.print(F(" Dateien in Ordner "));
  Serial.println(myFolder->folder);
#endif

  // Hörspielmodus: eine zufällige Datei aus dem Ordner
  if (myFolder->mode == 1) {
#ifdef DEBUG
    Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
#endif
    currentTrack = random(1, numTracksInFolder + 1);
#ifdef DEBUG
    Serial.println(currentTrack);
#endif
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  // Album Modus: kompletten Ordner spielen
  if (myFolder->mode == 2) {
#ifdef DEBUG
    Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
#endif
    currentTrack = 1;
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  // Party Modus: Ordner in zufälliger Reihenfolge
  if (myFolder->mode == 3) {
#ifdef DEBUG
    Serial.println(
      F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
#endif
    shuffleQueue();
    currentTrack = 1;
    PlayFolderMP3(myFolder->folder, queue[currentTrack - 1]);
  }
  // Einzel Modus: eine Datei aus dem Ordner abspielen
  if (myFolder->mode == 4) {
#ifdef DEBUG
    Serial.println(
      F("Einzel Modus -> eine Datei aus dem Odrdner abspielen"));
#endif
    currentTrack = myFolder->special;
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  // Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
  if (myFolder->mode == 5) {
#ifdef DEBUG
    Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
                     "Fortschritt merken"));
#endif
    currentTrack = EEPROM.read(myFolder->folder);
    if (currentTrack == 0 || currentTrack > numTracksInFolder) {
      currentTrack = 1;
    }
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
  // Spezialmodus Von-Bis: Hörspiel: eine zufällige Datei aus dem Ordner
  if (myFolder->mode == 7) {
#ifdef DEBUG
    Serial.println(F("Spezialmodus Von-Bis: Hörspiel -> zufälligen Track wiedergeben"));
    Serial.print(myFolder->special);
    Serial.print(F(" bis "));
    Serial.println(myFolder->special2);
#endif
    numTracksInFolder = myFolder->special2;
    currentTrack = random(myFolder->special, numTracksInFolder + 1);
#ifdef DEBUG
    Serial.println(currentTrack);
#endif
    PlayFolderMP3(myFolder->folder, currentTrack);
  }

  // Spezialmodus Von-Bis: Album: alle Dateien zwischen Start und Ende spielen
  if (myFolder->mode == 8) {
#ifdef DEBUG
    Serial.println(F("Spezialmodus Von-Bis: Album: alle Dateien zwischen Start- und Enddatei spielen"));
    Serial.print(myFolder->special);
    Serial.print(F(" bis "));
    Serial.println(myFolder->special2);
#endif
    numTracksInFolder = myFolder->special2;
    currentTrack = myFolder->special;
    PlayFolderMP3(myFolder->folder, currentTrack);
  }

  // Spezialmodus Von-Bis: Party Ordner in zufälliger Reihenfolge
  if (myFolder->mode == 9) {
#ifdef DEBUG
    Serial.println(
      F("Spezialmodus Von-Bis: Party -> Ordner in zufälliger Reihenfolge wiedergeben"));
#endif
    firstTrack = myFolder->special;
    numTracksInFolder = myFolder->special2;
    shuffleQueue();
    currentTrack = 1;
    PlayFolderMP3(myFolder->folder, queue[currentTrack - 1]);
  }
  // Spezialmodus Puzzel: Spiel das nach Auflegen einer Karte die dazugehörige Gegenkarte erwartet. Puzzel Start Karte muss zuerst aufgelegt werden.
  if (myFolder->mode == 10) {
#ifdef DEBUG
    Serial.println(
      F("Spezialmodus Puzzle"));
#endif
    currentTrack = myFolder->special;
    PlayFolderMP3(myFolder->folder, currentTrack);
  }
// Spezialmodus Von-Bis: Hörbuch: kompletten Ordner spielen und Fortschritt merken (nur für ein Hörbuch zur Zeit je Ordner)
  if (myFolder->mode == 11) {
#ifdef DEBUG    
    Serial.println(
      F("Spezialmodus Von-Bis: Hörbuch -> kompletten Ordner spielen und Fortschritt merken (nur für ein Hörbuch zur Zeit je Ordner)"));
#endif   
    firstTrack = myFolder->special;
    numTracksInFolder = myFolder->special2;
  currentTrack = EEPROM.read(myFolder->folder);
  if(currentTrack < firstTrack || currentTrack >= firstTrack + numTracksInFolder) {
    currentTrack = firstTrack;
#ifdef DEBUG  
    Serial.println(F("Der gespeicherte Fortschritt stammt nicht von diesem Hörbuch. Starte am Anfang."));
#endif
  }
      PlayFolderMP3(myFolder->folder, currentTrack);
  }
  while (!isPlaying() && counter <= 50) {
    delay(100);
    counter ++;
    if (counter >= 50) {
#ifdef DEBUG
      Serial.println(F("== playFolder() -> Track is not starting.")) ;
#endif
    }
  }
}
//////////////////////////////////////////////////////////////////////////
void playShortCut(uint8_t shortCut) {
#ifdef DEBUG
  Serial.println(F("=== playShortCut()"));
  Serial.println(shortCut);
#endif
  if (mySettings.shortCuts[shortCut].folder != 0) {
    myFolder = &mySettings.shortCuts[shortCut];
    playFolder();
    disablestandbyTimer();
    //delay(1000);
  }
  else
  {
#ifdef DEBUG
    Serial.println(F("Shortcut not configured!"));
#endif
  }
}
//////////////////////////////////////////////////////////////////////////
#ifdef CHECK_BATTERY
void batteryCheck () {
  float physValue;

  physValue =  (((analogRead(batMeasPin) * refVoltage) / pinSteps) / (Rtwo / (Rone + Rtwo)));

  Bat_Mean = Bat_Mean * Bat_N / (Bat_N + 1) + physValue / (Bat_N + 1);
  if ((Bat_N < 5) || (Bat_VarMean > 0.002)) {
    Bat_SqrMean = Bat_SqrMean * Bat_N / (Bat_N + 1) + physValue * physValue / (Bat_N + 1);
    Bat_VarMean = (Bat_SqrMean - Bat_Mean * Bat_Mean) / (Bat_N + 1);
    Bat_N = Bat_N + 1;
  }
  if (Bat_N > 10)  {
    if (Bat_Mean > batLevel_LEDyellow)    {
      batLevel_LEDyellowCounter = 0;
      batLevel_LEDyellow = batLevel_LEDyellowOn;
      setColor(0, 10, 0); // Green Color
      //#ifdef DEBUG
      //  Serial.println(F("=== batteryCheck() -> Battery High"));
      //#endif
    }
    else if (Bat_Mean < batLevel_LEDyellow && Bat_Mean > batLevel_LEDred )    {
      if (batLevel_LEDyellowCounter >= 10)      {
        batLevel_LEDredCounter = 0;
        batLevel_LEDyellowCounter = 0;
        batLevel_LEDyellow = batLevel_LEDyellowOff;
        batLevel_LEDred = batLevel_LEDredOn;
        setColor(20, 10, 0); // Yellow Color
        #ifdef DEBUG2
          Serial.println(F("=== batteryCheck() -> Battery Mid"));
        #endif
        }
      else
        batLevel_LEDyellowCounter ++;
    }

    else if (Bat_Mean < batLevel_LEDred && Bat_Mean > batLevel_Empty)    {
      if (batLevel_LEDredCounter >= 10)      {
        batLevel_EmptyCounter = 0;
        batLevel_LEDredCounter = 0;
        batLevel_LEDred = batLevel_LEDredOff;
        setColor(20, 0, 0); // Red Color
        #ifdef DEBUG2
          Serial.println(F("=== batteryCheck() -> Battery Low"));
        #endif
      }
      else
        batLevel_LEDredCounter ++;
    }
    else if (Bat_Mean <= batLevel_Empty)    {
      if (batLevel_EmptyCounter >= 10)      {
#ifdef DEBUG2
        Serial.println(F("=== batteryCheck() -> Battery Empty"));
#endif
        batLevel_EmptyCounter = 0;
        //shutDown();
      }
      else
        batLevel_EmptyCounter++;
    }
  }
}
#endif
//////////////////////////////////////////////////////////////////////////
void shutDown () {  
   if (activeModifier != NULL)
     if (activeModifier->handleShutDown() == true)
        return;
        
#ifdef DEBUG
  Serial.println("Shut Down");
#endif

#ifdef STARTUP_SOUND
  mp3.pause();
  delay(500);
  mp3.setVolume(mySettings.initVolume);
  delay(500);
#ifdef DEBUG2
  Serial.println("Shut Down Sound");
#endif
  mp3.playMp3FolderTrack(265);
  delay(1000);
  waitForTrackToFinish();
#endif

  digitalWrite(shutdownPin, HIGH);
}
//////////////////////////////////////////////////////////////////////////
void loop() {
  do {
#ifdef ROTARY_ENCODER
    encPos += encoder.getValue();
    
    if ((encPos >= oldEncPos +2) || (encPos <= oldEncPos -2))  {
      if (encPos >= oldEncPos +2) {        
        encPos = encPos -1;
        oldEncPos = encPos;
      }
      else if(encPos <= oldEncPos -2){        
        encPos = encPos +1;
        oldEncPos = encPos;
      }            
      if (encPos > (mySettings.maxVolume)) {
        volume  = mySettings.maxVolume;
        encPos  = mySettings.maxVolume;
      }
      else if (encPos < (mySettings.minVolume)) {
        volume  = mySettings.minVolume;
        encPos  = mySettings.minVolume;
      }
      else   {        
        volume = encPos;
      }
      mp3.setVolume(volume);
    }
#endif
    checkStandbyAtMillis();
    mp3.loop();

#ifdef CHECK_BATTERY
    batteryCheck();
#endif

    // Buttons werden nun über JS_Button gehandelt, dadurch kann jede Taste
    // doppelt belegt werden
    readButtons();

    // Modifier : WIP!
    if (activeModifier != NULL) {
      activeModifier->loop();
    }

    // admin menu
    if ((pauseButton.pressedFor(LONG_PRESS) || upButton.pressedFor(LONG_PRESS) || downButton.pressedFor(LONG_PRESS)) && pauseButton.isPressed() && upButton.isPressed() && downButton.isPressed()) {
      mp3.pause();
      do {
        readButtons();
      } while (pauseButton.isPressed() || upButton.isPressed() || downButton.isPressed());
      readButtons();
      adminMenu();
      break;
    }

    if (pauseButton.wasReleased()) {

      if (activeModifier != NULL)
        if (activeModifier->handlePause() == true)
          return;
      if (ignorePauseButton == false)

        if (isPlaying()) {
          mp3.pause();
          setstandbyTimer();
        }
        else if (knownCard) {
          mp3.start();
          disablestandbyTimer();
        }
      ignorePauseButton = false;
    }
#ifdef PUSH_ON_OFF
    else if (pauseButton.pressedFor(LONG_PRESS) &&
             ignorePauseButton == false) {
      ignorePauseButton = true;
      shutDown();
    }
#endif

#ifndef ROTARY_ENCODER
    if (upButton.pressedFor(LONG_PRESS)) {

#ifndef FIVEBUTTONS
      if (isPlaying()) {
        if (!mySettings.invertVolumeButtons) {
          volumeUpButton();
        }
        else {
          nextButton();
        }
      }
      else {
        playShortCut(1);
      }
      ignoreUpButton = true;
#endif
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton)
        if (!mySettings.invertVolumeButtons) {
          nextButton();
        }
        else {
          volumeUpButton();
        }
      ignoreUpButton = false;
    }
    if (downButton.pressedFor(LONG_PRESS)) {
#ifndef FIVEBUTTONS
      if (isPlaying()) {
        if (!mySettings.invertVolumeButtons) {
          volumeDownButton();
        }
        else {
          previousButton();
        }
      }
      else {
        playShortCut(2);
      }
      ignoreDownButton = true;
#endif
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        if (!mySettings.invertVolumeButtons) {
          previousButton();
        }
        else {
          volumeDownButton();
        }
      }
      ignoreDownButton = false;
    }


#ifdef FIVEBUTTONS
    if (buttonFour.wasReleased()) {
      if (isPlaying()) {
        if (!mySettings.invertVolumeButtons) {
          volumeUpButton();
        }
        else {
          nextButton();
        }
      }
      else {
        playShortCut(1);
      }
    }
    if (buttonFive.wasReleased()) {
      if (isPlaying()) {
        if (!mySettings.invertVolumeButtons) {
          volumeDownButton();
        }
        else {
          previousButton();
        }
      }
      else {
        playShortCut(2);
      }
    }
#endif

#endif

#ifdef ROTARY_ENCODER
    if (upButton.pressedFor(LONG_PRESS)) {
      if (isPlaying()) {
        nextButton();
      }

      else {
        playShortCut(1);
      }
      ignoreUpButton = true;
    }


    else if (upButton.wasReleased()) {
      if (!ignoreUpButton)
        if (isPlaying()) {
          nextButton();
        }
      ignoreUpButton = false;
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      if (isPlaying()) {
        previousButton();
      }

      else {
        playShortCut(2);
      }
      ignoreDownButton = true;
    }


    else if (downButton.wasReleased()) {
      if (!ignoreDownButton)
        if (isPlaying()) {
          previousButton();
        }
      ignoreDownButton = false;
    }
#endif

    // Ende der Buttons
  } while (!mfrc522.PICC_IsNewCardPresent());

  // RFID Karte wurde aufgelegt

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  if (readCard(&myCard) == true) {
    if (myCard.cookie == cardCookie && myCard.nfcFolderSettings.folder != 0 && myCard.nfcFolderSettings.mode != 0) {
      playFolder();
    }

    // Neue Karte konfigurieren
    else if (myCard.cookie != cardCookie) {
      knownCard = false;
      mp3.playMp3FolderTrack(300);
      waitForTrackToFinish();
      setupCard();
    }
  }
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
//////////////////////////////////////////////////////////////////////////
void adminMenu(bool fromCard = false) {
  disablestandbyTimer();
  mp3.pause();
#ifdef DEBUG
  Serial.println(F("=== adminMenu()"));
#endif
  knownCard = false;
  if (fromCard == false) {
    // Admin menu has been locked - it still can be trigged via admin card
    if (mySettings.adminMenuLocked == 1) {
      return;
    }
    // Pin check
    else if (mySettings.adminMenuLocked == 2) {
      uint8_t pin[4];
      mp3.playMp3FolderTrack(991);
      if (askCode(pin) == true) {
        if (checkTwo(pin, mySettings.adminMenuPin) == false) {
          return;
        }
      } else {
        return;
      }
    }
    // Match check
    else if (mySettings.adminMenuLocked == 3) {
      uint8_t a = random(10, 20);
      uint8_t b = random(1, 10);
      uint8_t c;
      mp3.playMp3FolderTrack(992);
      waitForTrackToFinish();
      mp3.playMp3FolderTrack(a);

      if (random(1, 3) == 2) {
        // a + b
        c = a + b;
        waitForTrackToFinish();
        mp3.playMp3FolderTrack(993);
      } else {
        // a - b
        b = random(1, a);
        c = a - b;
        waitForTrackToFinish();
        mp3.playMp3FolderTrack(994);
      }
      waitForTrackToFinish();
      mp3.playMp3FolderTrack(b);
#ifdef DEBUG
      Serial.println(c);
#endif
      uint8_t temp = voiceMenu(255, 0, 0, false);
      if (temp != c) {
        return;
      }
    }
  }
  int subMenu = voiceMenu(12, 900, 900, false, false, 0, true);
  if (subMenu == 0) {
    return;
  }
  if (subMenu == 1) {
    resetCard();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  else if (subMenu == 2) {
    // Maximum Volume
    mySettings.maxVolume = voiceMenu(30 - mySettings.minVolume, 930, mySettings.minVolume, false, false, mySettings.maxVolume - mySettings.minVolume) + mySettings.minVolume;
  }
  else if (subMenu == 3) {
    // Minimum Volume
    mySettings.minVolume = voiceMenu(mySettings.maxVolume - 1, 931, 0, false, false, mySettings.minVolume);
  }
  else if (subMenu == 4) {
    // Initial Volume
    mySettings.initVolume = voiceMenu(mySettings.maxVolume - mySettings.minVolume + 1, 932, mySettings.minVolume - 1, false, false, mySettings.initVolume - mySettings.minVolume + 1) + mySettings.minVolume - 1;
  }
  else if (subMenu == 5) {
    // EQ
    mySettings.eq = voiceMenu(6, 920, 920, false, false, mySettings.eq);
    mp3.setEq(mySettings.eq - 1);
  }
  else if (subMenu == 6) {
    // create modifier card
    nfcTagObject tempCard;
    tempCard.cookie = cardCookie;
    tempCard.version = 1;
    tempCard.nfcFolderSettings.folder = 0;
    tempCard.nfcFolderSettings.special = 0;
    tempCard.nfcFolderSettings.special2 = 0;
    tempCard.nfcFolderSettings.mode = voiceMenu(10, 969, 969, false, false, 0, true);

    if (tempCard.nfcFolderSettings.mode != 0) {
      if (tempCard.nfcFolderSettings.mode == 1) {
        switch (voiceMenu(4, 960, 960)) {
          case 1: tempCard.nfcFolderSettings.special = 5; break;
          case 2: tempCard.nfcFolderSettings.special = 15; break;
          case 3: tempCard.nfcFolderSettings.special = 30; break;
          case 4: tempCard.nfcFolderSettings.special = 60; break;
        }
      } else if (tempCard.nfcFolderSettings.mode == 8) {
        switch (voiceMenu(2, 1, 1)) {
          case 1: tempCard.nfcFolderSettings.special = 0; break;
          case 2: tempCard.nfcFolderSettings.special = 1; break;
        }
      }
      else if (tempCard.nfcFolderSettings.mode == 9) {
        tempCard.nfcFolderSettings.special =  voiceMenu(99, 301, 0, true, 0, 0, true);
      }
      else if (tempCard.nfcFolderSettings.mode == 10) {
        tempCard.nfcFolderSettings.special =  voiceMenu(99, 301, 0, true, 0, 0, true);
        tempCard.nfcFolderSettings.special2 =  voiceMenu(30, 904, 0, true, 0, 0, true);
      }
      mp3.playMp3FolderTrack(800);
      do {
        readButtons();
        if (upButton.wasReleased() || downButton.wasReleased()) {
#ifdef DEBUG
          Serial.println(F("Abgebrochen!"));
#endif
          mp3.playMp3FolderTrack(802);
          return;
        }
      } while (!mfrc522.PICC_IsNewCardPresent());

      // RFID Karte wurde aufgelegt
      if (mfrc522.PICC_ReadCardSerial()) {
#ifdef DEBUG
        Serial.println(F("schreibe Karte..."));
#endif
        writeCard(tempCard);
        delay(100);
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        waitForTrackToFinish();
      }
    }
  }
  else if (subMenu == 7) {
    uint8_t shortcut = voiceMenu(4, 940, 940);
    setupFolder(&mySettings.shortCuts[shortcut - 1]);
    mp3.playMp3FolderTrack(400);
  }
  else if (subMenu == 8) {
    switch (voiceMenu(5, 960, 960)) {
      case 1: mySettings.standbyTimer = 5; break;
      case 2: mySettings.standbyTimer = 15; break;
      case 3: mySettings.standbyTimer = 30; break;
      case 4: mySettings.standbyTimer = 60; break;
      case 5: mySettings.standbyTimer = 0; break;
    }
  }
  else if (subMenu == 9) {
    // Create Cards for Folder
    // Ordner abfragen
    nfcTagObject tempCard;
    tempCard.cookie = cardCookie;
    tempCard.version = 1;
    tempCard.nfcFolderSettings.mode = 4;
    tempCard.nfcFolderSettings.folder = voiceMenu(99, 301, 0, true);
    uint8_t special = voiceMenu(getFolderMP3Count(tempCard.nfcFolderSettings.folder), 321, 0,
                                true, tempCard.nfcFolderSettings.folder);
    uint8_t special2 = voiceMenu(getFolderMP3Count(tempCard.nfcFolderSettings.folder), 322, 0,
                                 true, tempCard.nfcFolderSettings.folder, special);

    mp3.playMp3FolderTrack(936);
    waitForTrackToFinish();
    for (uint8_t x = special; x <= special2; x++) {
      mp3.playMp3FolderTrack(x);
      tempCard.nfcFolderSettings.special = x;
#ifdef DEBUG
      Serial.print(x);
      Serial.println(F(" Karte auflegen"));
#endif
      do {
        readButtons();
        if (upButton.wasReleased() || downButton.wasReleased()) {
#ifdef DEBUG
          Serial.println(F("Abgebrochen!"));
#endif
          mp3.playMp3FolderTrack(802);
          return;
        }
      } while (!mfrc522.PICC_IsNewCardPresent());

      // RFID Karte wurde aufgelegt
      if (mfrc522.PICC_ReadCardSerial()) {
#ifdef DEBUG
        Serial.println(F("schreibe Karte..."));
#endif
        writeCard(tempCard);
        delay(100);
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        waitForTrackToFinish();
      }
    }
  }
  else if (subMenu == 10) {
    // Invert Functions for Up/Down Buttons
    int temp = voiceMenu(2, 933, 933, false);
    if (temp == 2) {
      mySettings.invertVolumeButtons = true;
    }
    else {
      mySettings.invertVolumeButtons = false;
    }
  }
  else if (subMenu == 11) {
#ifdef DEBUG
    Serial.println(F("Reset -> EEPROM wird gelöscht"));
#endif
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.update(i, 0);
    }
    resetSettings();
    mp3.playMp3FolderTrack(999);
  }
  // lock admin menu
  else if (subMenu == 12) {
    int temp = voiceMenu(4, 980, 980, false);
    if (temp == 1) {
      mySettings.adminMenuLocked = 0;
    }
    else if (temp == 2) {
      mySettings.adminMenuLocked = 1;
    }
    else if (temp == 3) {
      int8_t pin[4];
      mp3.playMp3FolderTrack(991);
      if (askCode(pin)) {
        memcpy(mySettings.adminMenuPin, pin, 4);
        mySettings.adminMenuLocked = 2;
      }
    }
    else if (temp == 4) {
      mySettings.adminMenuLocked = 3;
    }

  }
  writeSettingsToFlash();
  setstandbyTimer();
}
//////////////////////////////////////////////////////////////////////////
bool askCode(uint8_t *code) {
  uint8_t x = 0;
  while (x < 4) {
    readButtons();
    if (pauseButton.pressedFor(LONG_PRESS))
      break;
    if (pauseButton.wasReleased())
      code[x++] = 1;
    if (upButton.wasReleased())
      code[x++] = 2;
    if (downButton.wasReleased())
      code[x++] = 3;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////
uint8_t voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
                  bool preview = false, int previewFromFolder = 0, int defaultValue = 0, bool exitWithLongPress = false) {
  uint8_t returnValue = defaultValue;
  if (startMessage != 0)
    mp3.playMp3FolderTrack(startMessage);
#ifdef DEBUG2
  Serial.print(F("=== voiceMenu() ("));
  Serial.print(numberOfOptions);
  Serial.println(F(" Options)"));
#endif
  do {
#ifdef DEBUG2
    if (Serial.available() > 0) {
      int optionSerial = Serial.parseInt();
      if (optionSerial != 0 && optionSerial <= numberOfOptions)
        return optionSerial;
    }
#endif
    readButtons();
    mp3.loop();
    if (pauseButton.pressedFor(LONG_PRESS)) {
      mp3.playMp3FolderTrack(802);
      ignorePauseButton = true;
      return defaultValue;
    }
    if (pauseButton.wasReleased()) {
      if (returnValue != 0) {
#ifdef DEBUG2
        Serial.print(F("=== "));
        Serial.print(returnValue);
        Serial.println(F(" ==="));
#endif
        return returnValue;
      }
      delay(500);
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      returnValue = min(returnValue + 10, numberOfOptions);
#ifdef DEBUG2
      Serial.println(returnValue);
#endif
      //mp3.pause();
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      waitForTrackToFinish();
      /*if (preview) {
        if (previewFromFolder == 0)
          PlayFolderMP3(returnValue, 1);
        else
          PlayFolderMP3(previewFromFolder, returnValue);
        }*/
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        returnValue = min(returnValue + 1, numberOfOptions);
#ifdef DEBUG2
        Serial.println(returnValue);
#endif
        //mp3.pause();
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        if (preview) {
          waitForTrackToFinish();
          if (previewFromFolder == 0) {
            PlayFolderMP3(returnValue, 1);
          } else {
            PlayFolderMP3(previewFromFolder, returnValue);
          }
          delay(500);
        }
      } else {
        ignoreUpButton = false;
      }
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      returnValue = max(returnValue - 10, 1);
#ifdef DEBUG2
      Serial.println(returnValue);
#endif
      //mp3.pause();
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      waitForTrackToFinish();
      /*if (preview) {
        if (previewFromFolder == 0)
          PlayFolderMP3(returnValue, 1);
        else
          PlayFolderMP3(previewFromFolder, returnValue);
        }*/
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        returnValue = max(returnValue - 1, 1);
#ifdef DEBUG2
        Serial.println(returnValue);
#endif
        //mp3.pause();
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        if (preview) {
          waitForTrackToFinish();
          if (previewFromFolder == 0) {
            PlayFolderMP3(returnValue, 1);
          }
          else {
            PlayFolderMP3(previewFromFolder, returnValue);
          }
          delay(500);
        }
      } else {
        ignoreDownButton = false;
      }
    }
  } while (true);
}
//////////////////////////////////////////////////////////////////////////
void resetCard() {
  mp3.playMp3FolderTrack(800);
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();

    if (upButton.wasReleased() || downButton.wasReleased()) {
#ifdef DEBUG
      Serial.print(F("Abgebrochen!"));
#endif
      mp3.playMp3FolderTrack(802);
      return;
    }
  } while (!mfrc522.PICC_IsNewCardPresent());

  if (!mfrc522.PICC_ReadCardSerial())
    return;

#ifdef DEBUG
  Serial.print(F("Karte wird neu konfiguriert!"));
#endif
  setupCard();
}
//////////////////////////////////////////////////////////////////////////
bool setupFolder(folderSettings * theFolder) {
  // Ordner abfragen
  theFolder->folder = voiceMenu(99, 301, 0, true, 0, 0, true);
  if (theFolder->folder == 0) return false;

  // Wiedergabemodus abfragen
  theFolder->mode = voiceMenu(10, 309, 309, false, 0, 0, true);
  if (theFolder->mode == 0) return false;

  //  // Hörbuchmodus -> Fortschritt im EEPROM auf 1 setzen
  //  EEPROM.update(theFolder->folder, 1);

  // Einzelmodus -> Datei abfragen
  if (theFolder->mode == 4)
    theFolder->special = voiceMenu(getFolderMP3Count(theFolder->folder), 320, 0,
                                   true, theFolder->folder);
  // Admin Funktionen
  if (theFolder->mode == 6) {
    //theFolder->special = voiceMenu(3, 320, 320);
    theFolder->folder = 0;
    theFolder->mode = 255;
  }
  // Spezialmodus Von-Bis
  if (theFolder->mode == 7 || theFolder->mode == 8 || theFolder->mode == 9 || theFolder->mode == 11) {
    theFolder->special = voiceMenu(getFolderMP3Count(theFolder->folder), 321, 0,
                                   true, theFolder->folder);
    theFolder->special2 = voiceMenu(getFolderMP3Count(theFolder->folder), 322, 0,
                                    true, theFolder->folder, theFolder->special);
  }
  //Puzzle ode Quiz Karte
  if (theFolder->mode == 10 ) {
    theFolder->special = voiceMenu(getFolderMP3Count(theFolder->folder), 323, 0,
                                   true, theFolder->folder);
    theFolder->special2 = voiceMenu(255, 324, 0,
                                    false, theFolder->folder, 0);

  }

  return true;
}
//////////////////////////////////////////////////////////////////////////
void setupCard() {
  mp3.pause();
  Serial.println(F("=== setupCard()"));
  nfcTagObject newCard;
  if (setupFolder(&newCard.nfcFolderSettings) == true)
  {
    // Karte ist konfiguriert -> speichern
    mp3.pause();
    do {
    } while (isPlaying());
    writeCard(newCard);
  }
  delay(1000);
}

bool readCard(nfcTagObject * nfcTag) {
  nfcTagObject tempCard;
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
#ifdef DEBUG2
  Serial.println();
  Serial.print(F("PICC type: "));
#endif
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
#ifdef DEBUG2
  Serial.println(mfrc522.PICC_GetTypeName(piccType));
#endif

  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key A
  if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
      (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
      (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
  {
#ifdef DEBUG
    Serial.println(F("Authenticating Classic using key A..."));
#endif
    status = mfrc522.PCD_Authenticate(
               MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  }
  else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL )
  {
    byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the tempCard

    // Authenticate using key A
#ifdef DEBUG
    Serial.println(F("Authenticating MIFARE UL..."));
#endif
    status = mfrc522.PCD_NTAG216_AUTH(key.keyByte, pACK);
  }

  if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
#endif
    return false;
  }

  // Show the whole sector as it currently is
  // Serial.println(F("Current data in sector:"));
  // mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
  // Serial.println();

  // Read data from the block
  if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
      (piccType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
      (piccType == MFRC522::PICC_TYPE_MIFARE_4K ) )
  {
#ifdef DEBUG2
    Serial.print(F("Reading data from block "));
    Serial.print(blockAddr);
    Serial.println(F(" ..."));
#endif
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
  }
  else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL )
  {
    byte buffer2[18];
    byte size2 = sizeof(buffer2);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(8, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
      Serial.print(F("MIFARE_Read_1() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(9, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
      Serial.print(F("MIFARE_Read_2() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 4, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(10, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
      Serial.print(F("MIFARE_Read_3() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 8, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(11, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
      Serial.print(F("MIFARE_Read_4() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 12, buffer2, 4);
  }

#ifdef DEBUG
  Serial.print(F("Data on Card "));
  Serial.println(F(":"));
#endif
  dump_byte_array(buffer, 16);
#ifdef DEBUG
  Serial.println();
  Serial.println();
#endif

  uint32_t tempCookie;
  tempCookie = (uint32_t)buffer[0] << 24;
  tempCookie += (uint32_t)buffer[1] << 16;
  tempCookie += (uint32_t)buffer[2] << 8;
  tempCookie += (uint32_t)buffer[3];

  tempCard.cookie = tempCookie;
  tempCard.version = buffer[4];
  tempCard.nfcFolderSettings.folder = buffer[5];
  tempCard.nfcFolderSettings.mode = buffer[6];
  tempCard.nfcFolderSettings.special = buffer[7];
  tempCard.nfcFolderSettings.special2 = buffer[8];

  if (tempCard.cookie == cardCookie) {

    if (activeModifier != NULL && tempCard.nfcFolderSettings.folder != 0) {
      if (activeModifier->handleRFID(&tempCard) == true) {
        return false;
      }
    }

    if (tempCard.nfcFolderSettings.folder == 0) {
      if (activeModifier != NULL) {
        if (activeModifier->getActive() == tempCard.nfcFolderSettings.mode) {
          activeModifier = NULL;
#ifdef DEBUG
          Serial.println(F("modifier removed"));
#endif
          if (isPlaying()) {
            mp3.playAdvertisement(261);
          }
          else {
            //            mp3.start();
            //            delay(100);
            //            mp3.playAdvertisement(261);
            //            delay(100);
            //            mp3.pause();
            mp3.pause();
            delay(500);
            mp3.playMp3FolderTrack(261);
            waitForTrackToFinish();
          }
          //delay(2000);
          return false;
        }
      }
      if (tempCard.nfcFolderSettings.mode != 0 && tempCard.nfcFolderSettings.mode != 255) {
        if (isPlaying()) {
          mp3.playAdvertisement(260);
        }
        else {
          //          mp3.start();
          //          delay(100);
          //          mp3.playAdvertisement(260);
          //          delay(100);
          //          mp3.pause();
          //mp3.pause();
          //delay(500);
          mp3.playMp3FolderTrack(260);
          //delay(1000);
          waitForTrackToFinish();
        }
      }
      delay(2000);
      switch (tempCard.nfcFolderSettings.mode ) {
        case 0:
        case 255:
          mfrc522.PICC_HaltA(); mfrc522.PCD_StopCrypto1(); adminMenu(true);  break;
        case 1: activeModifier = new SleepTimer(tempCard.nfcFolderSettings.special); break;
        case 2: activeModifier = new FreezeDance(); break;
        case 3: activeModifier = new Locked(); break;
        case 4: activeModifier = new ToddlerMode(); break;
        case 5: activeModifier = new KindergardenMode(); break;
        case 6: activeModifier = new RepeatSingleModifier(); break;
        case 7: activeModifier = new FeedbackModifier(); break;
        case 8: activeModifier = new PuzzleGame(tempCard.nfcFolderSettings.special); break;
        case 9: activeModifier = new QuizGame(tempCard.nfcFolderSettings.special); break;
        case 10: activeModifier = new ButtonSmash(tempCard.nfcFolderSettings.special, tempCard.nfcFolderSettings.special2); break;
      }
      //delay(2000);
      return false;
    }
    else {
      memcpy(nfcTag, &tempCard, sizeof(nfcTagObject));
#ifdef DEBUG2
      Serial.println( nfcTag->nfcFolderSettings.folder);
#endif
      myFolder = &nfcTag->nfcFolderSettings;
#ifdef DEBUG2
      Serial.println( myFolder->folder);
#endif
    }
    return true;
  }
  else {
    memcpy(nfcTag, &tempCard, sizeof(nfcTagObject));
    return true;
  }
}
//////////////////////////////////////////////////////////////////////////
void writeCard(nfcTagObject nfcTag) {
  MFRC522::PICC_Type mifareType;
  byte buffer[16] = {0x13, 0x37, 0xb3, 0x47, // 0x1337 0xb347 magic cookie to
                     // identify our nfc tags
                     0x02,                   // version 1
                     nfcTag.nfcFolderSettings.folder,          // the folder picked by the user
                     nfcTag.nfcFolderSettings.mode,    // the playback mode picked by the user
                     nfcTag.nfcFolderSettings.special, // track or function for admin cards
                     nfcTag.nfcFolderSettings.special2,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                    };

  byte size = sizeof(buffer);

  mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  // Authenticate using key B
  //authentificate with the card and set card specific parameters
  if ((mifareType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_4K ) )
  {
#ifdef DEBUG2
    Serial.println(F("Authenticating again using key A..."));
#endif
    status = mfrc522.PCD_Authenticate(
               MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  }
  else if (mifareType == MFRC522::PICC_TYPE_MIFARE_UL )
  {
    byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

    // Authenticate using key A
#ifdef DEBUG2
    Serial.println(F("Authenticating UL..."));
#endif
    status = mfrc522.PCD_NTAG216_AUTH(key.keyByte, pACK);
  }

  if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
#endif
    mp3.playMp3FolderTrack(401);
    waitForTrackToFinish();
    return;
  }

  // Write data to the block
#ifdef DEBUG2
  Serial.print(F("Writing data into block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
#endif
  dump_byte_array(buffer, 16);
#ifdef DEBUG2
  Serial.println();
#endif

  if ((mifareType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_4K ) )
  {
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(blockAddr, buffer, 16);
  }
  else if (mifareType == MFRC522::PICC_TYPE_MIFARE_UL )
  {
    byte buffer2[16];
    byte size2 = sizeof(buffer2);

    memset(buffer2, 0, size2);
    memcpy(buffer2, buffer, 4);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(8, buffer2, 16);

    memset(buffer2, 0, size2);
    memcpy(buffer2, buffer + 4, 4);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(9, buffer2, 16);

    memset(buffer2, 0, size2);
    memcpy(buffer2, buffer + 8, 4);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(10, buffer2, 16);

    memset(buffer2, 0, size2);
    memcpy(buffer2, buffer + 12, 4);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(11, buffer2, 16);
  }

  if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG2
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
#endif
    mp3.playMp3FolderTrack(401);
  }
  else
    mp3.playMp3FolderTrack(400);
#ifdef DEBUG2
  Serial.println();
#endif
  waitForTrackToFinish();
  //delay(2000);
}
//////////////////////////////////////////////////////////////////////////
/**
  Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte * buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
#ifdef DEBUG2
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
#endif
  }
}
///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo ( uint8_t a[], uint8_t b[] ) {
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] ) {     // IF a != b then false, because: one fails, all fail
      return false;
    }
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////
#ifdef ROTARY_ENCODER
void timerIsr()
{
  encoder.service();
}
#endif

void PlayFolderMP3 (uint8_t folder, uint8_t track) {
  uint8_t new_folder = 0;
  
  if (folder > 99) {
    new_folder = folder - 99;}
  else {
    new_folder = folder;  }
    
  mp3.playFolderTrack(new_folder,track);
}
uint8_t getFolderMP3Count (uint8_t folder) {
  uint8_t new_folder = 0;
  
  if (folder > 99) {
    new_folder = folder - 99;}
  else {
    new_folder = folder;  }
    
  return mp3.getFolderTrackCount(new_folder);
}
