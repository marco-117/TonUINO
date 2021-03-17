#include <DFMiniMp3.h>
#include <EEPROM.h>
#include <JC_Button.h>
#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>

/*
   _____         _____ _____ _____ _____
  |_   _|___ ___|  |  |     |   | |     |
    | | | . |   |  |  |-   -| | | |  |  |
    |_| |___|_|_|_____|_____|_|___|_____|
    TonUINO Version 2.1
    created by Thorsten Voß and licensed under GNU/GPL.
    Information and contribution at https://tonuino.de.
    Fork by Marco Schulz
*/

///////// uncomment the below line to enable the function ////////////////
//#define FIVEBUTTONS
#define DEBUG
#define DEBUG_2
//#define DEBUG_QUEUE
//#define PUSH_ON_OFF
#define STARTUP_SOUND
//#define SPEAKER_SWITCH
#define ROTARY_ENCODER
//#define ROTARY_SWITCH
//#define POWER_ON_LED
//#define FADING_LED //Experimentell, nur in Verbindung mit POWER_ON_LED
//////////////////////////////////////////////////////////////////////////

#ifdef ROTARY_ENCODER
#include <TimerOne.h>
#include <ClickEncoder.h>
#endif

///////// conifguration of the input and output pin //////////////////////
#define buttonPause A0 //Default A0; Pocket A2
#define buttonUp A1 //Default A1; Pocket A0
#define buttonDown A2 //Default A2; Pocket A1
#define busyPin 4

#define shutdownPin 7 //Default 7

#define openAnalogPin A7 //Default A7

#ifdef FIVEBUTTONS
#define buttonFourPin A3
#define buttonFivePin A4
#endif

#define LONG_PRESS 1000
#define LONGER_PRESS 2000
#define LONGEST_PRESS 5000

#ifdef SPEAKER_SWITCH
#define SpeakerOnPin 8
#endif

#ifdef POWER_ON_LED
#define PowerOnLEDPin 6 //Default 6
#endif

#ifdef ROTARY_ENCODER
#define ROTARY_ENCODER_PIN_A 5 //Default 5; 
#define ROTARY_ENCODER_PIN_B 6 //Default 6; 
#define ROTARY_ENCODER_PIN_SUPPLY 8 //uncomment if you want to use an IO pin as supply
#endif

#ifdef ROTARY_SWITCH
#define ROTARY_SWITCH_PIN  A7
//#define ROTARY_SWITCH_SUPPLY_PIN 6
#endif
//////////////////////////////////////////////////////////////////////////

////////// NFC Gain //////////////////////////////////////////////////////
//#define NFCgain_max   // Maximale Empfindlichkeit
#define NFCgain_avg   // Mittlere Empfindlichkeit
//#define NFCgain_min   // Minimale Empfindlichkeit
//////////////////////////////////////////////////////////////////////////

///////// conifguration of the rotary encoder ////////////////////////////
#ifdef ROTARY_ENCODER
#define ROTARY_ENCODER_STEPS 4
//#define ROTARY_ENCODER_PRINT
#endif
//////////////////////////////////////////////////////////////////////////

///////// conifguration of the rotary switch ////////////////////////////
#ifdef ROTARY_SWITCH
#define ROTARY_SWITCH_POSITIONS 12
#define ROTARY_SWITCH_TOLERNACE 0.15
#define ROTARY_SWITCH_REF_VOLTAGE 5.0
#define ROTARY_SWITCH_STEP_MIN ((ROTARY_SWITCH_REF_VOLTAGE/ROTARY_SWITCH_POSITIONS) - ((ROTARY_SWITCH_REF_VOLTAGE/ROTARY_SWITCH_POSITIONS)*ROTARY_SWITCH_TOLERNACE))
#define ROTARY_SWITCH_STEP_MAX ((ROTARY_SWITCH_REF_VOLTAGE/ROTARY_SWITCH_POSITIONS) + ((ROTARY_SWITCH_REF_VOLTAGE/ROTARY_SWITCH_POSITIONS)*ROTARY_SWITCH_TOLERNACE))
#define ROTARY_SWITCH_TRIGGER_TIME 2000
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

const uint8_t folderMemoryCount = 8;

//////// rotary encoder /////////////////////////////////////////////////
#ifdef ROTARY_ENCODER
int16_t RotEncOldEncPos = 0 ;
int16_t RotEncPos = 0;
ClickEncoder encoder(ROTARY_ENCODER_PIN_A, ROTARY_ENCODER_PIN_B, ROTARY_ENCODER_STEPS);
#endif
//////////////////////////////////////////////////////////////////////////

//////// rotary encoder /////////////////////////////////////////////////
#ifdef ROTARY_SWITCH
uint8_t RotSwCurrentPos = 0;
unsigned long RotSwMillis = 0;
uint8_t RotSwNewPos = 0;
uint8_t RotSwActivePos = 0;
#endif

typedef enum Enum_PlayMode
{
  AudioDrama = 1,
  Album = 2,
  Party = 3,
  Single = 4,
  AudioBook = 5,
  AdminMenu = 6,
  AudioDrama_Section = 7,
  Album_Section = 8,
  Party_Section = 9,
  AudioBook_Section = 10,
  PuzzlePart = 11
};
typedef enum Enum_Modifier
{
  ModifierMode = 0,
  SleepTimerMod = 1,
  FreezeDanceMod = 2,
  LockedMod = 3,
  ToddlerModeMod = 4,
  KindergardenModeMod = 5,
  RepeatSingleMod = 6,
  FeedbackMod = 7,
  PuzzleGameMod = 8,
  QuizGameMod = 9,
  ButtonSmashMod = 10,
  AdminMenuMod = 255
};
typedef enum Enum_SystemControl
{
  SystemControl = 254,
  PauseSysCont = 1,
  VolumeSysCont = 2,
  ForwardSysCont = 3,
  BackwardSysCont = 4,
  ShutDownSysCont = 5,
  RemoveModifierSysCont = 6
};
typedef enum Enum_AdminMenuOptions
{
  Exit = 0,
  ResetCard = 1,
  MaxVolume = 2,
  MinVolume = 3,
  InitVolume = 4,
  EQ = 5,
  CreateModifier = 6,
  SetupShortCuts = 7,
  SetupStandbyTimer = 8,
  CreateFolderCards = 9,
  InvertButtons = 10,
  StopWhenCardAway = 11,
  SetupRotarySwitch = 12,
  ResetEEPROM = 13,
  LockAdminMenu = 14
};
typedef enum Enum_PCS {
  PCS_NO_CHANGE     = 0, // no change detected since last pollCard() call
  PCS_NEW_CARD      = 1, // card with new UID detected (had no card or other card before)
  PCS_CARD_GONE     = 2, // card is not reachable anymore
  PCS_CARD_IS_BACK  = 3 // card was gone, and is now back again
};

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
  uint8_t special3;
  uint8_t special4;
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
  folderSettings shortCuts[3];
#ifdef ROTARY_SWITCH
  folderSettings rotarySwitchSlots[ROTARY_SWITCH_POSITIONS];
#endif
  uint8_t adminMenuLocked;
  uint8_t adminMenuPin[4];
  folderSettings savedModifier;
  bool stopWhenCardAway;
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
#ifdef ROTARY_SWITCH
bool SetModifier (uint8_t tmpMode, uint8_t tmpSpecial1, uint8_t tmpSpecial2);
void RotSwloop(uint8_t TriggerTime = 0);
#endif

static bool hasCard = false;
static byte lastCardUid[4];
static byte retries;
static bool lastCardWasUL;
static bool forgetLastCard = false;

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
#ifdef DEBUG
      switch (errorCode) {
        case DfMp3_Error_Busy: {
            Serial.print(F("busy"));
            break;
          }
        case DfMp3_Error_Sleeping: {
            Serial.print(F("sleep"));
            break;
          }
        case DfMp3_Error_SerialWrongStack: {
            Serial.print(F("serial stack"));
            break;
          }
        case DfMp3_Error_CheckSumNotMatch: {
            Serial.print(F("checksum"));
            break;
          }
        case DfMp3_Error_FileIndexOut: {
            Serial.print(F("file index"));
            break;
          }
        case DfMp3_Error_FileMismatch: {
            Serial.print(F("file mismatch"));
            break;
          }
        case DfMp3_Error_Advertise: {
            Serial.print(F("advertise"));
            break;
          }
        case DfMp3_Error_RxTimeout: {
            Serial.print(F("rx timeout"));
            break;
          }
        case DfMp3_Error_PacketSize: {
            Serial.print(F("packet size"));
            break;
          }
        case DfMp3_Error_PacketHeader: {
            Serial.print(F("packet header"));
            break;
          }
        case DfMp3_Error_PacketChecksum: {
            Serial.print(F("packet checksum"));
            break;
          }
        case DfMp3_Error_General: {
            Serial.print(F("general"));
            break;
          }
        default: {
            Serial.print(F("unknown"));
            break;
          }
      }
      Serial.println(F(" error"));
#endif
      //      Serial.println();
      //      Serial.print("Com Error ");
      //      Serial.println(errorCode);
    }
    static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {
      if (source & DfMp3_PlaySources_Sd) Serial.print("SD ");
      if (source & DfMp3_PlaySources_Usb) Serial.print("USB ");
      if (source & DfMp3_PlaySources_Flash) Serial.print("Flash ");
      Serial.println(action);
    }
    static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track) {
      nextTrack(track);
    }
    static void OnPlaySourceOnline(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "online");
    }
    static void OnPlaySourceInserted(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "ready");
    }
    static void OnPlaySourceRemoved(DfMp3_PlaySources source) {
      PrintlnSourceAction(source, "removed");
    }
};

static DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(mySoftwareSerial);
//////////////////////////////////////////////////////////////////////////
void shuffleQueue(uint8_t startTrack = firstTrack, uint8_t endTrack = numTracksInFolder) {
#ifdef DEBUG
  Serial.println(F("shuffle Queue"));
#ifdef DEBUG_2
  Serial.print(F("startTrack: "));
  Serial.println(startTrack);
  Serial.print(F("endTrack: "));
  Serial.println(endTrack);
#endif
#endif
  // Queue für die Zufallswiedergabe erstellen
  for (uint8_t x = 0; x < endTrack - startTrack + 1; x++)
    queue[x] = x + startTrack;
  // Rest mit 0 auffüllen
  for (uint8_t x = endTrack - startTrack + 1; x < 255; x++)
    queue[x] = 0;
  // Queue mischen
  for (uint8_t i = 0; i < endTrack - startTrack + 1; i++)
  {
    uint8_t j = random (0, endTrack - startTrack + 1);
    uint8_t t = queue[i];
    queue[i] = queue[j];
    queue[j] = t;
  }
#ifdef DEBUG_QUEUE
  Serial.println(F("Queue :"));
  for (uint8_t x = 0; x < endTrack - startTrack + 1 ; x++) {
    Serial.print(x + 1);
    Serial.print(". : ");
    Serial.println(queue[x]);
  }
#endif
}
//////////////////////////////////////////////////////////////////////////
void writeSettingsToFlash() {
  int address = sizeof(myFolder->folder) * 99 * folderMemoryCount;
#if defined DEBUG
  Serial.print(F("write settings to flash at adress "));
  Serial.println(address);
#endif
  EEPROM.put(address, mySettings);
}
//////////////////////////////////////////////////////////////////////////
void loadSettingsFromFlash() {
  int address = sizeof(myFolder->folder) * 99 * folderMemoryCount;
  EEPROM.get(address, mySettings);

#if defined DEBUG
  Serial.print(F("settings in flash at address "));
  Serial.println(address);
#endif

  if (mySettings.cookie != cardCookie)
    resetSettings();
  migrateSettings(mySettings.version);

#ifdef DEBUG
  Serial.print(F("Version: "));
  Serial.println(mySettings.version);

  Serial.print(F("Max Vol: "));
  Serial.println(mySettings.maxVolume);

  Serial.print(F("Min Vole: "));
  Serial.println(mySettings.minVolume);

  Serial.print(F("Init Vol: "));
  Serial.println(mySettings.initVolume);

  Serial.print(F("EQ: "));
  Serial.println(mySettings.eq);

  Serial.print(F("Locked: "));
  Serial.println(mySettings.locked);

  Serial.print(F("Sleep Timer: "));
  Serial.println(mySettings.standbyTimer);

  Serial.print(F("Inverted Vol Buttons: "));
  Serial.println(mySettings.invertVolumeButtons);

  Serial.print(F("Admin Menu locked: "));
  Serial.println(mySettings.adminMenuLocked);

  Serial.print(F("Admin Menu Pin: "));
  Serial.print(mySettings.adminMenuPin[0]);
  Serial.print(mySettings.adminMenuPin[1]);
  Serial.print(mySettings.adminMenuPin[2]);
  Serial.println(mySettings.adminMenuPin[3]);

  Serial.print(F("Saved Modifier Mode: "));
  Serial.println(mySettings.savedModifier.mode);

#endif
}
//////////////////////////////////////////////////////////////////////////
void resetSettings() {
#ifdef DEBUG
  Serial.println(F("reset settings in EEPROM"));
#endif
  mySettings.cookie = cardCookie;
  mySettings.version = 2;
  mySettings.maxVolume = 25;
  mySettings.minVolume = 1;
  mySettings.initVolume = 8;
  mySettings.eq = 1;
  mySettings.locked = false;
  mySettings.standbyTimer = 0;
  mySettings.invertVolumeButtons = true;
  mySettings.shortCuts[0].folder = 0;
  mySettings.shortCuts[1].folder = 0;
  mySettings.shortCuts[2].folder = 0;
  mySettings.adminMenuLocked = 0;
  mySettings.adminMenuPin[0] = 1;
  mySettings.adminMenuPin[1] = 1;
  mySettings.adminMenuPin[2] = 1;
  mySettings.adminMenuPin[3] = 1;
  mySettings.savedModifier.folder = 0;
  mySettings.savedModifier.mode = 0;
  mySettings.stopWhenCardAway = false;
#ifdef ROTARY_SWITCH
  for (uint8_t i = 0; i <= ROTARY_SWITCH_POSITIONS - 1; i++) {
    mySettings.rotarySwitchSlots[i].folder = 0;
    mySettings.rotarySwitchSlots[i].mode = 0;
    mySettings.rotarySwitchSlots[i].special = 0;
    mySettings.rotarySwitchSlots[i].special2 = 0;
    mySettings.rotarySwitchSlots[i].special3 = 0;
    mySettings.rotarySwitchSlots[i].special4 = 0;
  }
#endif
  writeSettingsToFlash();
}
//////////////////////////////////////////////////////////////////////////
void migrateSettings(int oldVersion) {
  if (oldVersion == 1) {
#ifdef DEBUG
    Serial.println(F("migrate Settings"));
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
    virtual bool handleVolume() {
      return false;
    }
    virtual bool handleShortCut() {
      return false;
    }
    virtual bool handleShutDown() {
      return false;
    }
    virtual bool handleRFID(nfcTagObject *newCard) {
      return false;
    }
    virtual bool handleStopWhenCardAway() {
      return false;
    }
    virtual bool handleSaveModifier() {
      return false;
    }
    virtual bool handleAdminMenu() {
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
#ifdef DEBUG
        Serial.println(F("SleepTimer > sleep"));
#endif
        mp3.pause();
        setstandbyTimer();
        activeModifier = NULL;
        delete this;
      }
    }

    SleepTimer(uint8_t minutes) {
#ifdef DEBUG
      Serial.print(F("SleepTimer minutes: "));
      Serial.println(minutes);
#endif
      this->sleepAtMillis = millis() + minutes * 60000;
    }
    uint8_t getActive() {
      return SleepTimerMod;
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
#ifdef DEBUG
      Serial.print(F("FreezeDance set next stop at "));
      Serial.print(seconds);
      Serial.println(F(" s."));
#endif
      this->nextStopAtMillis = millis() + seconds * 1000;
    }

  public:
    void loop() {
      if (this->nextStopAtMillis != 0 && millis() > this->nextStopAtMillis) {
#ifdef DEBUG
        Serial.println(F("FreezeDance > freeze"));
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
      Serial.println(F("FreezeDance"));
#endif
      if (isPlaying()) {
        delay(1000);
        mp3.playAdvertisement(300);
        delay(500);
      }
      setNextStopAtMillis();
    }
    uint8_t getActive() {
      return FreezeDanceMod ;
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
      Serial.println(F("PuzzleGame > success"));
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
      Serial.println(F("PuzzleGame > Failure"));
#endif

      mp3.playMp3FolderTrack(296);
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
#ifdef DEBUG
        Serial.println(F("PuzzleGame > delete part"));
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
      Serial.println(F("PuzzleGame"));
#endif

      mode = special;
    }

    virtual bool handlePause() {
      return false;
    }

    virtual bool handleNext() {
      return true;
    }

    virtual bool handleNextButton()       {
      return true;
    }

    virtual bool handlePreviousButton() {
      return true;
    }

    virtual bool handleShutDown() {
      return true;
    }

    virtual bool handleShortCut() {
      return true;
    }

    virtual bool handleRFID(nfcTagObject *newCard) {

      this->tmpCard = *newCard;
      if (tmpCard.nfcFolderSettings.mode != PuzzlePart) {
#ifdef DEBUG
        Serial.println(F("PuzzleGame > no valid part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(299); //Diese Karte ist nicht Teil des Spiels. Probiere eine Andere.
        return true;
      }
      else {
#ifdef DEBUG
        Serial.println(F("PuzzleGame > valid part"));
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
    virtual bool handleStopWhenCardAway() {
      return true;
    }
    uint8_t getActive() {
      return PuzzleGameMod;
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
      Serial.println(F("QuizGame > success"));
#endif
      mp3.playMp3FolderTrack(297);
      waitForTrackToFinish();
      next();
    }

    void Failure () {
      PartTwoSaved = false;
#ifdef DEBUG
      Serial.println(F("QuizGame > failure"));
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
          CompareParts();
        }
      }

      if (upButton.pressedFor(LONG_PRESS) && downButton.pressedFor(LONG_PRESS)) {
        do {
          readButtons();
        } while (upButton.isPressed() || downButton.isPressed());
#ifdef DEBUG
        Serial.println(F("QuizGame > delete part"));
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
      Serial.println(F("QuizGame"));
#endif
      mp3.pause();

      PartOneFolder = special;
      numTracksInFolder = mp3.getFolderTrackCount(PartOneFolder);
      firstTrack = 1;
      shuffleQueue();
      currentTrack = 0;

#ifdef DEBUG
      Serial.println(F("QuizGame > queue set"));
#endif
      next();
    }

    void  next() {
      numTracksInFolder = mp3.getFolderTrackCount(PartOneFolder);

      if (currentTrack != numTracksInFolder - firstTrack + 1) {
#ifdef DEBUG
        Serial.println(F("QuizGame > queue next"));
#endif
        currentTrack++;
      } else {
#ifdef DEBUG
        Serial.println(F("QuizGame > queue repeat"));
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
      mp3.playFolderTrack(PartOneFolder, PartOneSpecial);
    }

    virtual bool handleNext() {
      return true;
    }

    virtual bool handlePause() {
#ifdef DEBUG
      Serial.println(F("QuizGame > pause and repeat "));
#endif
      mp3.pause();
      delay(100);
      mp3.playFolderTrack(PartOneFolder, PartOneSpecial);
      return true;
    }

    virtual bool handleNextButton()       {
      return true;
    }

    virtual bool handlePreviousButton() {
      return true;
    }

    virtual bool handleShutDown() {
      return true;
    }

    virtual bool handleShortCut() {
      return true;
    }

    virtual bool handleRFID(nfcTagObject * newCard) {
      this->tmpCard = *newCard;
      if (tmpCard.nfcFolderSettings.mode != PuzzlePart) {
#ifdef DEBUG
        Serial.println(F("QuizGame > no valid part"));
#endif
        mp3.pause();
        delay(100);
        mp3.playMp3FolderTrack(299); //Diese Karte ist nicht Teil des Spiels. Probiere eine Andere.
        return true;
      }
      else {
#ifdef DEBUG
        Serial.println(F("QuizGame > valid part"));
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
    virtual bool handleStopWhenCardAway() {
      return true;
    }
    uint8_t getActive() {
      return QuizGameMod;
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
      Serial.println(F("ButtonSmash"));
#endif
      mp3.pause();
      delay(200);

      mp3.setVolume(special2);
      delay(200);

      Folder = special;
      numTracksInFolder = mp3.getFolderTrackCount(Folder);

      firstTrack = 1;
      shuffleQueue();
      currentTrack = 0;
    }

    void  next() {
      mp3.pause();
      delay(100);
      numTracksInFolder = mp3.getFolderTrackCount(Folder);

      if (currentTrack != numTracksInFolder - firstTrack + 1) {
#ifdef DEBUG
        Serial.println(F("ButtonSmash > queue next"));
#endif
        currentTrack++;
      } else {
#ifdef DEBUG
        Serial.println(F("ButtonSmash > repeat queue"));
#endif
        firstTrack = 1;
        shuffleQueue();
        currentTrack = 1;
      }
      mp3.playFolderTrack(Folder, queue[currentTrack - 1]);
      waitForTrackToFinish();
    }

    virtual bool handleNext() {
      return true;
    }

    virtual bool handlePause() {
      next();
      return true;
    }

    virtual bool handleNextButton() {
      next();
      return true;
    }

    virtual bool handlePreviousButton() {
      next();
      return true;
    }

    virtual bool handleVolume() {
      next();
      return true;
    }

    virtual bool handleShortCut() {
      next();
      return true;
    }

    virtual bool handleShutDown() {
      next();
      return true;
    }

    virtual bool handleAdminMenu() {
      next();
      return true;
    }
    virtual bool handleRFID(nfcTagObject * newCard) {
      next();
      return true;
    }
    virtual bool handleStopWhenCardAway() {
      return true;
    }
    uint8_t getActive() {
      return ButtonSmashMod ;
    }
};
//////////////////////////////////////////////////////////////////////////
class Locked: public Modifier {
  public:
    virtual bool handlePause()     {
      return true;
    }
    virtual bool handleNextButton()       {
      return true;
    }
    virtual bool handlePreviousButton() {
      return true;
    }
    virtual bool handleVolume()   {
      return true;
    }
    virtual bool handleShutDown() {
      return true;
    }

    virtual bool handleShortCut() {
      return true;
    }
    virtual bool handleAdminMenu() {
      return true;
    }

    virtual bool handleRFID(nfcTagObject *newCard) {
      return true;
    }
    Locked(void) {
#ifdef DEBUG
      Serial.println(F("Locked Moifier"));
#endif
    }
    uint8_t getActive() {
      return LockedMod;
    }
};
//////////////////////////////////////////////////////////////////////////
class ToddlerMode: public Modifier {
  public:
    virtual bool handlePause()     {
      return true;
    }
    virtual bool handleNextButton()       {
      return true;
    }
    virtual bool handlePreviousButton() {
      return true;
    }
    virtual bool handleVolume()   {
      return true;
    }
    virtual bool handleShutDown() {
      return true;
    }
    virtual bool handleShortCut() {
      return true;
    }
    virtual bool handleAdminMenu() {
      return true;
    }

    ToddlerMode() {
#ifdef DEBUG
      Serial.println(F("ToddlerMode"));
#endif
    }

    uint8_t getActive() {
      return ToddlerModeMod ;
    }
};
//////////////////////////////////////////////////////////////////////////
class KindergardenMode: public Modifier {
  private:
    nfcTagObject nextCard;
    bool cardQueued = false;

  public:
    virtual bool handleNext() {
#ifdef DEBUG
      Serial.println(F("KindergardenMode > next"));
#endif
      if (this->cardQueued == true) {
        this->cardQueued = false;

        myCard = nextCard;
        myFolder = &myCard.nfcFolderSettings;
#ifdef DEBUG
        Serial.println(myFolder->folder);
        Serial.println(myFolder->mode);
#endif
        playFolder();
        return true;
      }
      return false;
    }
    virtual bool handleNextButton()       {
      return true;
    }
    virtual bool handlePreviousButton() {
      return true;
    }
    virtual bool handleShutDown() {
      return true;
    }
    virtual bool handleAdminMenu() {
      return true;
    }

    virtual bool handleShortCut() {
      if (isPlaying())
        return true;
      else
        return false;
    }

    virtual bool handleRFID(nfcTagObject * newCard) { // lot of work to do!
#ifdef DEBUG
      Serial.println(F("KindergardenMode > RFID queued"));
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
      Serial.println(F("KindergardenMode"));
#endif
    }
    uint8_t getActive() {
      return KindergardenModeMod;
    }
};
//////////////////////////////////////////////////////////////////////////
class RepeatSingleModifier: public Modifier {
  public:
    virtual bool handleNext() {
#ifdef DEBUG
      Serial.println(F("RepeatSingle > repeat track"));
#endif
      delay(50);
      if (isPlaying())
        return true;
      if (myFolder->mode == Party || myFolder->mode == Party_Section) {
        mp3.playFolderTrack(myFolder->folder, queue[currentTrack - 1]);
      }
      else {
        mp3.playFolderTrack(myFolder->folder, currentTrack);
      }

      _lastTrackFinished = 0;
      return true;
    }
    RepeatSingleModifier() {
#ifdef DEBUG
      Serial.println(F("RepeatSingle"));
#endif
    }
    uint8_t getActive() {
      return RepeatSingleMod;
    }
};
////////////////////////////////////////////////////////////////////////////
//// An modifier can also do somethings in addition to the modified action
//// by returning false (not handled) at the end
//// This simple FeedbackModifier will tell the volume before changing it and
//// give some feedback once a RFID card is detected.
//class FeedbackModifier: public Modifier {
//  public:
//    virtual bool handleRFID(nfcTagObject *newCard) {
//#ifdef DEBUG
//      Serial.println(F("Feedback > RFID"));
//#endif
//      return false;
//    }
//    uint8_t getActive() {
//      return FeedbackMod;
//    }
//};
//////////////////////////////////////////////////////////////////////////
// Leider kann das Modul selbst keine Queue abspielen, daher müssen wir selbst die Queue verwalten
static void nextTrack(uint16_t track) {
  bool queueTrack = false;
#ifdef DEBUG
  Serial.println(F("Next track"));
#endif
  if (activeModifier != NULL) {
    if (activeModifier->handleNext() == true) {
#ifdef DEBUG
      Serial.println(F("locked"));
#endif
      return;
    }
  }

  if (track == _lastTrackFinished || knownCard == false) {
#ifdef DEBUG
    Serial.println(F("no next track"));
#endif
    return;
  }
  _lastTrackFinished = track;

  switch (myFolder->mode) {
    case Album:
      if (currentTrack < numTracksInFolder)
        currentTrack = currentTrack + 1;
      else {
        currentTrack = 0;
        mp3.pause();
        setstandbyTimer();
        return;
      }
      break;
    case Album_Section:
      if (currentTrack < myFolder->special2)
        currentTrack = currentTrack + 1;
      else {
        currentTrack = firstTrack - 1;
        mp3.pause();
        setstandbyTimer();
        return;
      }
      break;

    case Party:
      if (currentTrack < numTracksInFolder) {
        currentTrack = currentTrack + 1;
      }
      else {
        currentTrack = 1;
        //// Wenn am Ende der Queue neu gemischt werden soll bitte die Zeilen wieder aktivieren
        //     Serial.println(F("Ende der Queue > mische neu"));
        //     shuffleQueue();
      }
      queueTrack = true;
      break;
    case Party_Section:
      if (currentTrack < (myFolder->special2 - firstTrack + 1)) {
        currentTrack = currentTrack + 1;
      }
      else {
        currentTrack = 1;
        //// Wenn am Ende der Queue neu gemischt werden soll bitte die Zeilen wieder aktivieren
        //     Serial.println(F("Ende der Queue > mische neu"));
        //     shuffleQueue(firstTrack,myFolder->special2);
      }
      queueTrack = true;
      break;

    case AudioBook:
      if (currentTrack < numTracksInFolder) {
        currentTrack = currentTrack + 1;
        writeAudiobookMemory (myFolder->folder, myFolder->special3, currentTrack);
      }
      else {
        writeAudiobookMemory (myFolder->folder, myFolder->special3, firstTrack);
        mp3.pause();
        setstandbyTimer();
        return;
      }

      break;
    case AudioBook_Section:
      if (currentTrack < myFolder->special2) {
        currentTrack = currentTrack + 1;
        writeAudiobookMemory (myFolder->folder, myFolder->special3, currentTrack);
      }
      else {
        writeAudiobookMemory (myFolder->folder, myFolder->special3, firstTrack);
        mp3.pause();
        setstandbyTimer();
        return;
      }
      break;

    default:
#ifdef DEBUG
      Serial.println(F("No next Track"));
#endif
      setstandbyTimer();
      delay(500);
      return;
      break;
  }

  disablestandbyTimer();

#ifdef DEBUG
  Serial.print("next track: ");
#endif

  if (queueTrack) {
    mp3.playFolderTrack(myFolder->folder, queue[currentTrack - 1]);
#ifdef DEBUG
    Serial.println(queue[currentTrack - 1]);
#endif
  }
  else {
    mp3.playFolderTrack(myFolder->folder, currentTrack);


#ifdef DEBUG
    Serial.println(currentTrack);
#endif
  }

  delay(500);
}
//////////////////////////////////////////////////////////////////////////
static void previousTrack() {
  bool queueTrack = false;

#ifdef DEBUG
  Serial.println(F("Previous track"));
#endif
  if (activeModifier != NULL) {
    if (activeModifier->handlePrevious() == true) {
#ifdef DEBUG
      Serial.println(F("locked"));
#endif
      return;
    }
  }
  _lastTrackFinished = 0;

  switch (myFolder->mode) {
    case Album:
      if (currentTrack > 1) {
        currentTrack = currentTrack - 1;
      }
      else {
        currentTrack = numTracksInFolder;
      }
      break;
    case Album_Section:
      if (currentTrack > firstTrack) {
        currentTrack = currentTrack - 1;
      }
      else {
        currentTrack = myFolder->special2 - firstTrack + 1;
      }
      break;

    case Party:
      if (currentTrack > 1) {
        currentTrack = currentTrack - 1;
      }
      else {
        currentTrack = numTracksInFolder;
      }
      queueTrack = true;
      break;
    case Party_Section:
      if (currentTrack > 1) {
        currentTrack = currentTrack - 1;
      }
      else {
        currentTrack = myFolder->special2 - firstTrack + 1;
      }
      queueTrack = true;
      break;

    case AudioBook:
      if (currentTrack > 1) {
        currentTrack = currentTrack - 1;
      } else {
        currentTrack = 1;
      }
      // Fortschritt im EEPROM abspeichern
      writeAudiobookMemory (myFolder->folder, myFolder->special3, currentTrack);
      break;

    case AudioBook_Section:
      if (currentTrack > firstTrack) {
        currentTrack = currentTrack - 1;

      } else {
        currentTrack = 1;
      }
      // Fortschritt im EEPROM abspeichern
      writeAudiobookMemory (myFolder->folder, myFolder->special3, currentTrack);
      break;

    default:
#ifdef DEBUG
      Serial.println(F("No previous Track"));
#endif
      setstandbyTimer();
      return;
      break;
  }

  disablestandbyTimer();

#ifdef DEBUG
  Serial.print("previous track: ");
#endif

  if (queueTrack) {
    mp3.playFolderTrack(myFolder->folder, queue[currentTrack - 1]);
#ifdef DEBUG
    Serial.println(queue[currentTrack - 1]);
#endif
  }
  else {
    mp3.playFolderTrack(myFolder->folder, currentTrack);
#ifdef DEBUG
    Serial.println(currentTrack);
#endif
  }

  delay(500);
}
//////////////////////////////////////////////////////////////////////////
bool isPlaying() {
  return !digitalRead(busyPin);
}
//////////////////////////////////////////////////////////////////////////
void waitForTrackToFinish() {
  unsigned long currentTime = millis();
#define TIMEOUT 1000
  do {
    mp3.loop();
  } while (!isPlaying() && millis() < currentTime + TIMEOUT);
  delay(800);
  do {
    mp3.loop();
  } while (isPlaying());
}
//////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200); // Es gibt ein paar Debug Ausgaben über die serielle Schnittstelle
  // Dieser Hinweis darf nicht entfernt werden
  //  Serial.println(F("\n _____         _____ _____ _____ _____"));
  //  Serial.println(F("|_   _|___ ___|  |  |     |   | |     |"));
  //  Serial.println(F(" | | | . |   |  |  |-   -| | | |  |  |"));
  //  Serial.println(F(" |_| |___|_|_|_____|_____|_|___|_____|\n"));
  Serial.println(F("TonUINO Version 2.1"));
  Serial.println(F("created by Thorsten Voß and licensed under GNU/GPL."));
  Serial.println(F("Information and contribution at https://tonuino.de.\n"));
  Serial.println(F("Fork by Marco Schulz"));

  analogReference(DEFAULT);

  // Busy Pin
  pinMode(busyPin, INPUT);
  mp3.begin();


  // Wert für randomSeed() erzeugen durch das mehrfache Sammeln von rauschenden LSBs eines offenen Analogeingangs
  uint32_t ADC_LSB;
  uint32_t ADCSeed;
  for (uint8_t i = 0; i < 128; i++) {
    ADC_LSB = analogRead(openAnalogPin) & 0x1;
    ADCSeed ^= ADC_LSB << (i % 32);
  }
  randomSeed(ADCSeed); // Zufallsgenerator initialisieren

#ifdef ROTARY_ENCODER
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder.setAccelerationEnabled(false);
#ifdef ROTARY_ENCODER_PIN_SUPPLY
  pinMode(ROTARY_ENCODER_PIN_SUPPLY, OUTPUT);
  digitalWrite(ROTARY_ENCODER_PIN_SUPPLY, HIGH);
#endif
#endif

#ifdef ROTARY_SWITCH
#ifdef ROTARY_SWITCH_SUPPLY_PIN
  pinMode(ROTARY_SWITCH_SUPPLY_PIN, OUTPUT);
  digitalWrite(ROTARY_SWITCH_SUPPLY_PIN, HIGH);
#endif
  RotSwCurrentPos = RotSwGetPosition();
  RotSwActivePos = RotSwCurrentPos;
  RotSwMillis = millis();
#endif

#ifdef SPEAKER_SWITCH
  pinMode(SpeakerOnPin, OUTPUT);
  digitalWrite(SpeakerOnPin, LOW);
#endif

#ifdef POWER_ON_LED
  pinMode(PowerOnLEDPin, OUTPUT);
  digitalWrite(PowerOnLEDPin, LOW);
#endif

  //#ifdef CHECK_BATTERY
  //  pinMode(LEDredPin, OUTPUT);
  //  pinMode(LEDgreenPin, OUTPUT);
  //
  //  setColor(0, 10, 0); // White Color
  //
  //  batLevel_LEDyellow = batLevel_LEDyellowOn;
  //  batLevel_LEDred = batLevel_LEDredOn;
  //#endif

  // load Settings from EEPROM
  loadSettingsFromFlash();

  // Set saved Modifier
  if (mySettings.savedModifier.mode != 0)
    SetModifier(mySettings.savedModifier);

  // activate standby timer
  setstandbyTimer();


  // NFC Leser initialisieren
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
#ifdef DEBUG
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader
#endif
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

#ifdef NFCgain_min
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_min);
  Serial.println(F("=== mfrc522-> RxGain_min === "));
#endif
#ifdef NFCgain_avg
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_avg);
  Serial.println(F("=== mfrc522-> RxGain_avg === "));
#endif
#ifdef NFCgain_max
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  Serial.println(F("=== mfrc522-> RxGain_max === "));
#endif

  pinMode(buttonPause, INPUT_PULLUP);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
#ifdef FIVEBUTTONS
  pinMode(buttonFourPin, INPUT_PULLUP);
  pinMode(buttonFivePin, INPUT_PULLUP);
#endif
  pinMode(shutdownPin, OUTPUT);
  digitalWrite(shutdownPin, LOW);


  // RESET --- ALLE DREI KNÖPFE BEIM STARTEN GEDRÜCKT HALTEN > alle EINSTELLUNGEN werden gelöscht
  if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW &&
      digitalRead(buttonDown) == LOW) {
#ifdef DEBUG
    Serial.println(F("Delete EEPROM"));
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

#ifdef POWER_ON_LED
  digitalWrite(PowerOnLEDPin, HIGH);
#endif


  delay(3000);
  mp3.loop();
  volume = mySettings.initVolume;
  mp3.setVolume(volume);
  mp3.setEq(mySettings.eq - 1);
  
#if defined ROTARY_ENCODER
  RotEncPos = volume;
  RotEncOldEncPos = RotEncPos;
#endif

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
  if (activeModifier != NULL) {
    if (activeModifier->handleVolume() == true) {
#ifdef DEBUG
      Serial.println(F("Volume locked"));
#endif
      return;
    }
  }

if (isPlaying()){
  if (volume < mySettings.maxVolume) {
    mp3.increaseVolume();
    volume++;
  }
#ifdef DEBUG
  Serial.print(F("volume Up: "));
  Serial.println(volume);
#endif
}
}
//////////////////////////////////////////////////////////////////////////
void volumeDownButton() {
  if (activeModifier != NULL) {
    if (activeModifier->handleVolume() == true) {
#ifdef DEBUG
      Serial.println(F("Volume locked"));
#endif
      return;
    }
  }
if (isPlaying()){
  if (volume > mySettings.minVolume) {
    mp3.decreaseVolume();
    volume--;
  }
#ifdef DEBUG
  Serial.print(F("volume Down: "));
  Serial.println(volume);
#endif
}
}
#endif
//////////////////////////////////////////////////////////////////////////
void nextButton() {
  if (activeModifier != NULL) {
    if (activeModifier->handleNextButton() == true) {
#ifdef DEBUG
      Serial.println(F("Next button locked"));
#endif
      return;
    }
  }
  nextTrack(random(65536));
  delay(300);
}
//////////////////////////////////////////////////////////////////////////
void previousButton() {
  if (activeModifier != NULL) {
    if (activeModifier->handlePreviousButton() == true) {
#ifdef DEBUG
      Serial.println(F("Previous button locked"));
#endif
      return;
    }
  }
  previousTrack();
  delay(300);
}
//////////////////////////////////////////////////////////////////////////
void playFolder() {
  uint8_t counter = 0;
  bool queueTrack = false;

#ifdef DEBUG
  Serial.println(F("playFolder")) ;
#endif

  mp3.loop();
  disablestandbyTimer();
  knownCard = true;
  _lastTrackFinished = 0;
  numTracksInFolder = mp3.getFolderTrackCount(myFolder->folder);
  firstTrack = 1;

#ifdef DEBUG
  Serial.print(numTracksInFolder);
  Serial.print(F(" tracks in folder "));
  Serial.println(myFolder->folder);
#endif

  switch (myFolder->mode) {
    case AudioDrama:
#ifdef DEBUG
      Serial.println(F("Audio Drama"));
#endif
      currentTrack = random(1, numTracksInFolder + 1);
      break;
    case AudioDrama_Section:
#ifdef DEBUG
      Serial.println(F("Audio Drama section"));
      Serial.print(myFolder->special);
      Serial.print(F(" to "));
      Serial.println(myFolder->special2);
#endif
      //numTracksInFolder = myFolder->special2;
      firstTrack = myFolder->special;
      currentTrack = random(myFolder->special, myFolder->special2 + 1);
      break;

    case Album:
#ifdef DEBUG
      Serial.println(F("Album"));
#endif
      currentTrack = 1;
      break;
    case Album_Section:
#ifdef DEBUG
      Serial.println(F("Album section"));
      Serial.print(myFolder->special);
      Serial.print(F(" to "));
      Serial.println(myFolder->special2);
#endif
      //numTracksInFolder = myFolder->special2;
      firstTrack = myFolder->special;
      currentTrack = myFolder->special;
      break;

    case Party:
#ifdef DEBUG
      Serial.println(F("Party"));
#endif
      currentTrack = 1;
      shuffleQueue();
      queueTrack = true;
      break;
    case Party_Section:
#ifdef DEBUG
      Serial.println(F("Party section"));
#endif
      currentTrack = 1;
      firstTrack = myFolder->special;
      //numTracksInFolder = myFolder->special2;
      shuffleQueue(firstTrack, myFolder->special2);
      queueTrack = true;
      break;

    case AudioBook:
#ifdef DEBUG
      Serial.println(F("Audio Book"));
#endif
      currentTrack = readAudiobookMemory(myFolder->folder, myFolder->special3);
      if (currentTrack == 0 || currentTrack > numTracksInFolder) {
        currentTrack = 1;
      }
      break;
    case AudioBook_Section:
#ifdef DEBUG
      Serial.println(F("Audio Book section"));
#endif
      firstTrack = myFolder->special;
      //numTracksInFolder = myFolder->special2;
      currentTrack = readAudiobookMemory(myFolder->folder, myFolder->special3);
      if (currentTrack < firstTrack || currentTrack >= firstTrack + numTracksInFolder) {
        currentTrack = firstTrack;
#ifdef DEBUG
        Serial.println(F("not correct tag, start from beginn"));
#endif
      }
      break;

    case Single:
#ifdef DEBUG
      Serial.println(F("Single"));
#endif
      currentTrack = myFolder->special;
      break;

    case PuzzlePart:
#ifdef DEBUG
      Serial.println(F("Puzzle Part"));
#endif
      currentTrack = myFolder->special;
      break;
  }

#ifdef DEBUG
  Serial.print(F("play track: "));
#endif

  if (queueTrack) {
    mp3.playFolderTrack(myFolder->folder, queue[currentTrack - 1]);
#ifdef DEBUG
    Serial.println(queue[currentTrack - 1]);
#endif
  }
  else {
    mp3.playFolderTrack(myFolder->folder, currentTrack);
#ifdef DEBUG
    Serial.println(currentTrack);
#endif
  }

  while (!isPlaying() && counter <= 100) {
    delay(50);
    counter ++;
    if (counter >= 100) {
#ifdef DEBUG
      Serial.println(F("Track not starting")) ;
#endif
    }
  }
}
//////////////////////////////////////////////////////////////////////////
void playShortCut(uint8_t shortCut) {
#ifdef DEBUG
  Serial.print(F("Shortcut: "));
  Serial.println(shortCut);
#endif
  if (activeModifier != NULL) {
    if (activeModifier->handleShortCut() == true) {
#ifdef DEBUG
      Serial.println(F("Shortcut locked"));
#endif
      return;
    }
  }
  if (mySettings.shortCuts[shortCut].folder != 0) {
    myFolder = &mySettings.shortCuts[shortCut];
    playFolder();
    //disablestandbyTimer();
    //delay(1000);
  }
  else
  {
#ifdef DEBUG
    Serial.println(F("not configured"));
#endif
  }
}
//////////////////////////////////////////////////////////////////////////
void loop() {
  //  do {
  checkStandbyAtMillis();
  mp3.loop();

#ifdef ROTARY_ENCODER
  RotEncSetVolume();
#endif

  //#ifdef CHECK_BATTERY
  //    batteryCheck();
  //#endif

#ifdef ROTARY_SWITCH
  RotSwloop(ROTARY_SWITCH_TRIGGER_TIME);
#endif

#if defined FADING_LED
  fadeStatusLed(isPlaying());
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
    return;
  }

  //Springe zum ersten Titel zurück
  if ((upButton.pressedFor(LONGER_PRESS) || downButton.pressedFor(LONGER_PRESS)) && !pauseButton.isPressed() && upButton.isPressed() && downButton.isPressed()) {
    mp3.pause();
    do {
      readButtons();
    } while (upButton.isPressed() || downButton.isPressed());
    readButtons();
#if defined DEBUG
    Serial.println(F("back to first track"));
#endif
    if (currentTrack != firstTrack) {
      currentTrack = firstTrack;
      if (myFolder->special3 != 0) {
#if defined DEBUG
        Serial.println(F("reset memory"));
#endif
        writeAudiobookMemory (myFolder->folder, myFolder->special3, currentTrack);
      }
    }
    playFolder();
    return;
  }

  if (pauseButton.wasReleased()) {

    if (activeModifier != NULL) {
      if (activeModifier->handlePause() == true) {
#ifdef DEBUG
        Serial.println(F("Pause locked"));
#endif
        return;
      }
    }
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
  else if (pauseButton.pressedFor(LONGER_PRESS) &&
           (!upButton.isPressed() || !downButton.isPressed()) &&
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


#if defined FIVEBUTTONS
  if (buttonFour.wasReleased()) {
    if (isPlaying()) {
      if (!mySettings.invertVolumeButtons) {
        volumeUpButton();
      }
      else {
        nextButton();
      }
    }
  }     else if (buttonFour.pressedFor(LONG_PRESS)) {
    if (!isPlaying()) {
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
  } else if (buttonFive.pressedFor(LONG_PRESS)) {
    if (!isPlaying()) {
      playShortCut(2);
    }
  }
#endif

#endif

#ifdef ROTARY_ENCODER
  if (upButton.pressedFor(LONG_PRESS)) {
    if (!isPlaying())
      playShortCut(1);

    ignoreUpButton = true;
  }

  else if (upButton.wasReleased()) {
    if (!ignoreUpButton)
      nextButton();

    ignoreUpButton = false;
  }

  if (downButton.pressedFor(LONG_PRESS)) {
    if (!isPlaying()) {
      playShortCut(2);
    }
    ignoreDownButton = true;
  }

  else if (downButton.wasReleased()) {
    if (!ignoreDownButton)
      previousButton();

    ignoreDownButton = false;
  }

#endif

  handleCardReader();

  //    // Ende der Buttons
  //  } while (!mfrc522.PICC_IsNewCardPresent());
  //
  //  // RFID Karte wurde aufgelegt
  //
  //  if (!mfrc522.PICC_ReadCardSerial())
  //    return;
  //
  //  if (readCard(&myCard) == true) {
  //    if (myCard.cookie == cardCookie && myCard.nfcFolderSettings.folder != ModifierMode && myCard.nfcFolderSettings.mode != 0) {
  //      playFolder();
  //    }
  //
  //    // Neue Karte konfigurieren
  //    else if (myCard.cookie != cardCookie) {
  //      knownCard = false;
  //      mp3.playMp3FolderTrack(300);
  //      waitForTrackToFinish();
  //      setupCard();
  //    }
  //  }
  //  mfrc522.PICC_HaltA();
  //  mfrc522.PCD_StopCrypto1();
}
//////////////////////////////////////////////////////////////////////////
void adminMenu(bool fromCard = false) {
  if (activeModifier != NULL) {
    if (activeModifier->handleAdminMenu() == true) {
#ifdef DEBUG
      Serial.println(F("Admin Menu locked"));
#endif
      return;
    }
  }
  forgetLastCard = true;
  disablestandbyTimer();
  mp3.pause();
#ifdef DEBUG
  Serial.println(F("adminMenu"));
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
      if (voiceMenu(255, 0, 0, false) != c)
        return;
    }
  }

  int subMenu = voiceMenu(14, 900, 900, false, false, 0, true);
  if (subMenu == Exit)
    return;
  if (subMenu == ResetCard) {
    resetCard();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  else if (subMenu == MaxVolume) {
    // Maximum Volume
    mySettings.maxVolume = voiceMenu(30 - mySettings.minVolume, 930, mySettings.minVolume, false, false, mySettings.maxVolume - mySettings.minVolume) + mySettings.minVolume;
  }
  else if (subMenu == MinVolume) {
    // Minimum Volume
    mySettings.minVolume = voiceMenu(mySettings.maxVolume - 1, 931, 0, false, false, mySettings.minVolume);
  }
  else if (subMenu == InitVolume) {
    // Initial Volume
    mySettings.initVolume = voiceMenu(mySettings.maxVolume - mySettings.minVolume + 1, 932, mySettings.minVolume - 1, false, false, mySettings.initVolume - mySettings.minVolume + 1) + mySettings.minVolume - 1;
  }
  else if (subMenu == EQ) {
    // EQ
    mySettings.eq = voiceMenu(6, 920, 920, false, false, mySettings.eq);
    mp3.setEq(mySettings.eq - 1);
  }
  else if (subMenu == CreateModifier) {
    // create modifier card
    nfcTagObject tempCard;
    tempCard.cookie = cardCookie;
    tempCard.version = 2;

    if (setupModifier(&tempCard.nfcFolderSettings)) {
      mp3.playMp3FolderTrack(800);
      do {
        readButtons();
        if (upButton.wasReleased() || downButton.wasReleased()) {
#ifdef DEBUG
          Serial.println(F("abort"));
#endif
          mp3.playMp3FolderTrack(802);
          return;
        }
      } while (!mfrc522.PICC_IsNewCardPresent());

      // RFID Karte wurde aufgelegt
      if (mfrc522.PICC_ReadCardSerial()) {
#ifdef DEBUG
        Serial.println(F("write Tag"));
#endif
        writeCard(tempCard);
        delay(100);
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        waitForTrackToFinish();
      }
    }
  }
  else if (subMenu == SetupShortCuts) {
    uint8_t shortcut = voiceMenu(3, 940, 940);
    setupFolder(&mySettings.shortCuts[shortcut - 1]);
    mp3.playMp3FolderTrack(400);
  }
  else if (subMenu == SetupStandbyTimer) {
    switch (voiceMenu(5, 960, 960)) {
      case 1: mySettings.standbyTimer = 5; break;
      case 2: mySettings.standbyTimer = 15; break;
      case 3: mySettings.standbyTimer = 30; break;
      case 4: mySettings.standbyTimer = 60; break;
      case 5: mySettings.standbyTimer = 0; break;
    }
  }
  else if (subMenu == CreateFolderCards) {
    // Create Cards for Folder
    // Ordner abfragen
    nfcTagObject tempCard;
    tempCard.cookie = cardCookie;
    tempCard.version = 1;
    tempCard.nfcFolderSettings.mode = Single;
    tempCard.nfcFolderSettings.folder = voiceMenu(99, 301, 0, true);
    uint8_t special = voiceMenu(mp3.getFolderTrackCount(tempCard.nfcFolderSettings.folder), 321, 0,
                                true, tempCard.nfcFolderSettings.folder);
    uint8_t special2 = voiceMenu(mp3.getFolderTrackCount(tempCard.nfcFolderSettings.folder), 322, 0,
                                 true, tempCard.nfcFolderSettings.folder, special);

    mp3.playMp3FolderTrack(936);
    waitForTrackToFinish();
    for (uint8_t x = special; x <= special2; x++) {
      mp3.playMp3FolderTrack(x);
      tempCard.nfcFolderSettings.special = x;
#ifdef DEBUG
      Serial.print(x);
      Serial.println(F(" place Tag"));
#endif
      do {
        readButtons();
        if (upButton.wasReleased() || downButton.wasReleased()) {
#ifdef DEBUG
          Serial.println(F("abort!"));
#endif
          mp3.playMp3FolderTrack(802);
          return;
        }
      } while (!mfrc522.PICC_IsNewCardPresent());

      // RFID Karte wurde aufgelegt
      if (mfrc522.PICC_ReadCardSerial()) {
#ifdef DEBUG
        Serial.println(F("write Tag"));
#endif
        writeCard(tempCard);
        delay(100);
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        waitForTrackToFinish();
      }
    }
  }
  else if (subMenu == InvertButtons) {
    // Invert Functions for Up/Down Buttons
    switch (voiceMenu(2, 933, 933, false)) {
      case 1:
        mySettings.invertVolumeButtons = false;
        break;
      case 2:
        mySettings.invertVolumeButtons = true;
        break;
    }
  }
  else if (subMenu == StopWhenCardAway) {
    switch (voiceMenu(2, 937, 933, false)) {
      case 1:
        mySettings.stopWhenCardAway = false;
        break;
      case 2:
        mySettings.stopWhenCardAway = true;
        break;
    }
  }
  // lock admin menu
  else if (subMenu == SetupRotarySwitch) {
#ifdef ROTARY_SWITCH
    uint8_t rotSwSlot = 0;
    do {
      rotSwSlot = voiceMenu(ROTARY_SWITCH_POSITIONS, 945, 0);
      switch (voiceMenu(3, 948, 948)) {
        case 1:
          setupFolder(&mySettings.rotarySwitchSlots[rotSwSlot - 1]);
          break;
        case 2:
          setupModifier(&mySettings.rotarySwitchSlots[rotSwSlot - 1]);
          break;
        case 3:
          setupSystemControl(&mySettings.rotarySwitchSlots[rotSwSlot - 1]);
          break;
      }
      mp3.playMp3FolderTrack(400);
    } while (voiceMenu(2, 946, 933, false, false, 1, true) == 2);
#endif
#ifndef ROTARY_SWITCH
    mp3.playMp3FolderTrack(947);
#endif
  }
  else if (subMenu == ResetEEPROM) {
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.update(i, 0);
    }
    resetSettings();
    mp3.playMp3FolderTrack(999);
  }
  else if (subMenu == LockAdminMenu) {
    switch (voiceMenu(4, 980, 980, false)) {
      case 1:
        mySettings.adminMenuLocked = 0;
        break;
      case 2:
        mySettings.adminMenuLocked = 1;
        break;
      case 3:
        int8_t pin[4];
        mp3.playMp3FolderTrack(991);
        if (askCode(pin)) {
          memcpy(mySettings.adminMenuPin, pin, 4);
          mySettings.adminMenuLocked = 2;
        }
        break;
      case 4:
        mySettings.adminMenuLocked = 3;
        break;
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
  uint8_t returnValueOld = returnValue;
  if (startMessage != 0)
    mp3.playMp3FolderTrack(startMessage);
#ifdef DEBUG
  Serial.print(F("voiceMenu "));
  Serial.print(numberOfOptions);
  Serial.println(F(" Options"));
#endif
  do {
#ifdef DEBUG
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
      delay(300);
      ignorePauseButton = true;
      checkStandbyAtMillis();
      return defaultValue;
    }
    if (pauseButton.wasReleased()) {
      if (returnValue != 0) {
#ifdef DEBUG
        Serial.print(F("= "));
        Serial.println(returnValue);
#endif
        mp3.pause();
        //waitForTrackToFinish();
        return returnValue;
      }
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      returnValue = min(returnValue + 10, numberOfOptions);
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        returnValue = min(returnValue + 1, numberOfOptions);
      } else {
        ignoreUpButton = false;
      }
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      returnValue = max(returnValue - 10, 1);
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        returnValue = max(returnValue - 1, 1);
      } else {
        ignoreDownButton = false;
      }
    }

    if (returnValue != returnValueOld) {
      returnValueOld = returnValue;
#ifdef DEBUG
      Serial.println(returnValue);
#endif
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      if (preview) {
        //waitForTrackToFinish();
        if (previewFromFolder == 0) {
          mp3.playFolderTrack(returnValue, 1);
        } else {
          mp3.playFolderTrack(previewFromFolder, returnValue);
        }
        delay(300);
      }
    }
    delay(150);
  } while (true);
}
//////////////////////////////////////////////////////////////////////////
void resetCard() {
  mp3.playMp3FolderTrack(800);
  do {
    readButtons();
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
  Serial.print(F("reset Tag"));
#endif
  setupCard();
}
//////////////////////////////////////////////////////////////////////////
bool setupFolder(folderSettings * theFolder) {
  // Ordner abfragen
  theFolder->folder = voiceMenu(99, 301, 0, true, 0, 0, true);
  if (theFolder->folder == 0) return false;

  // Wiedergabemodus abfragen
  theFolder->mode = voiceMenu(11, 305, 305, false, 0, 0, true);
  if (theFolder->mode == 0) return false;
#ifdef DEBUG
  Serial.println(F("Setup folder"));
  Serial.print(F("Folder: "));
  Serial.println(theFolder->folder);
  Serial.print(F("Mode: "));
  Serial.println(theFolder->mode);
#endif
  //  // Hörbuchmodus > Fortschritt im EEPROM auf 1 setzen
  //  writeAudiobookMemory (myFolder->folder,myFolder->special3, 1);

  switch (theFolder->mode) {
    case Single:
#ifdef DEBUG
      Serial.println(F("Single"));
#endif
      //Einzeltrack in special speichern
      theFolder->special = voiceMenu(mp3.getFolderTrackCount(theFolder->folder), 320, 0,
                                     true, theFolder->folder);
      break;
    case AudioDrama_Section:
    case Album_Section:
    case Party_Section:
    case AudioBook_Section:
#ifdef DEBUG
      Serial.println(F("Section"));
#endif
      //Von (special), Bis (special2) speichern
      theFolder->special = voiceMenu(mp3.getFolderTrackCount(theFolder->folder), 321, 0,
                                     true, theFolder->folder);
      theFolder->special2 = voiceMenu(mp3.getFolderTrackCount(theFolder->folder), 322, 0,
                                      true, theFolder->folder, theFolder->special);
      //Speicherplatz für das Hörbuch wählen
      if (theFolder->mode == AudioBook_Section)
        theFolder->special3 = voiceMenu(folderMemoryCount, 325, 0,
                                        false, 0, 0, true);
      break;
    case AdminMenu:
#ifdef DEBUG
      Serial.println(F("AdminMenu"));
#endif
      theFolder->folder = 0;
      theFolder->mode = 255;
      break;
    case AudioBook:
#ifdef DEBUG
      Serial.println(F("AudioBook"));
#endif
      //Speicherplatz für das Hörbuch wählen
      theFolder->special3 = voiceMenu(8, 325, 0,
                                      false, 0, 0, true);
      break;
    case PuzzlePart:
#ifdef DEBUG
      Serial.println(F("PuzzlePart"));
#endif
      theFolder->special = voiceMenu(mp3.getFolderTrackCount(theFolder->folder), 323, 0,
                                     true, theFolder->folder);
      theFolder->special2 = voiceMenu(255, 324, 0,
                                      false, theFolder->folder);
      break;
    default:
      break;
  }
#ifdef DEBUG
  Serial.println(F("Setup folder ok"));
#endif
  return true;
}
//////////////////////////////////////////////////////////////////////////
void setupCard() {
  mp3.pause();
#ifdef DEBUG
  Serial.println(F("setupCard"));
#endif
  nfcTagObject newCard;
  if (setupFolder(&newCard.nfcFolderSettings) == true)
  {
    // Karte ist konfiguriert > speichern
    mp3.pause();
    do {
    } while (isPlaying());
    forgetLastCard = true;
    writeCard(newCard);
  }
  delay(1000);
}

bool readCard(nfcTagObject * nfcTag) {
  nfcTagObject tempCard;
  // Show some details of the PICC (that is: the tag/card)
#ifdef DEBUG
  Serial.println(F("Card UID "));
#endif
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
#ifdef DEBUG
  Serial.print(F("PICC type "));
#endif
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
#ifdef DEBUG
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
    Serial.print(F("PCD_Authenticate failed "));
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
#ifdef DEBUG
    Serial.print(F("Reading data block"));
    Serial.print(blockAddr);
    Serial.println(F("..."));
#endif
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
      Serial.print(F("MIFARE_Read failed: "));
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
#ifdef DEBUG
      Serial.print(F("MIFARE_Read_1 failed "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(9, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
      Serial.print(F("MIFARE_Read_2 failed "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 4, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(10, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
      Serial.print(F("MIFARE_Read_3 failed "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 8, buffer2, 4);

    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(11, buffer2, &size2);
    if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
      Serial.print(F("MIFARE_Read_4 failed "));
      Serial.println(mfrc522.GetStatusCodeName(status));
#endif
      return false;
    }
    memcpy(buffer + 12, buffer2, 4);
  }

#ifdef DEBUG
  Serial.print(F("Data on Card : "));
#endif
  dump_byte_array(buffer, 16);

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
  tempCard.nfcFolderSettings.special3 = buffer[9];
  tempCard.nfcFolderSettings.special4 = buffer[10];

  if (tempCard.cookie == cardCookie) {

    if (activeModifier != NULL && tempCard.nfcFolderSettings.folder != 0) {
      if (activeModifier->handleRFID(&tempCard) == true) {
#ifdef DEBUG
        Serial.println(F("RFID locked"));
#endif
        return false;
      }
    }
    if (tempCard.nfcFolderSettings.folder == 0)
      return SetModifier (tempCard.nfcFolderSettings);
    else {
      memcpy(nfcTag, &tempCard, sizeof(nfcTagObject));
#ifdef DEBUG
      Serial.println( nfcTag->nfcFolderSettings.folder);
#endif
      myFolder = &nfcTag->nfcFolderSettings;
#ifdef DEBUG
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
                     nfcTag.nfcFolderSettings.special3,
                     nfcTag.nfcFolderSettings.special4,
                     0x00, 0x00, 0x00, 0x00, 0x00
                    };

  byte size = sizeof(buffer);

#ifdef DEBUG
  Serial.println(F("write Card"));
#endif
  mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  // Authenticate using key B
  //authentificate with the card and set card specific parameters
  if ((mifareType == MFRC522::PICC_TYPE_MIFARE_MINI ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_1K ) ||
      (mifareType == MFRC522::PICC_TYPE_MIFARE_4K ) )
  {
#ifdef DEBUG
    Serial.println(F("Authenticating again using key A..."));
#endif
    status = mfrc522.PCD_Authenticate(
               MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  }
  else if (mifareType == MFRC522::PICC_TYPE_MIFARE_UL )
  {
    byte pACK[] = {0, 0}; //16 bit PassWord ACK returned by the NFCtag

    // Authenticate using key A
#ifdef DEBUG
    Serial.println(F("Authenticating UL..."));
#endif
    status = mfrc522.PCD_NTAG216_AUTH(key.keyByte, pACK);
  }

  if (status != MFRC522::STATUS_OK) {
#ifdef DEBUG
    Serial.print(F("PCD_Authenticate failed "));
    Serial.println(mfrc522.GetStatusCodeName(status));
#endif
    mp3.playMp3FolderTrack(401);
    waitForTrackToFinish();
    return;
  }

  // Write data to the block
#ifdef DEBUG
  Serial.print(F("Writing data "));
  Serial.print(blockAddr);
  Serial.println(F("..."));
#endif
  dump_byte_array(buffer, 16);
#ifdef DEBUG
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
#ifdef DEBUG
    Serial.print(F("MIFARE_Write failed "));
    Serial.println(mfrc522.GetStatusCodeName(status));
#endif
    mp3.playMp3FolderTrack(401);
  }
  else
    mp3.playMp3FolderTrack(400);
#ifdef DEBUG
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
#ifdef DEBUG
    Serial.print(buffer[i] < 0x10 ? "0" : " ");
    Serial.print(buffer[i], HEX );
    Serial.print(" ");
#endif
  }
#ifdef DEBUG
  Serial.println();
#endif
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
void RotEncSetVolume () {

  RotEncPos += encoder.getValue();

  if (RotEncPos != RotEncOldEncPos)  {
#if defined ROTARY_ENCODER_PRINT
    Serial.print(F("RotEncPos: "));
    Serial.println(RotEncPos);
    Serial.print(F("RotEncOldEncPos: "));
    Serial.println(RotEncOldEncPos);
#endif

    RotEncOldEncPos = RotEncPos;
    
    if (RotEncPos > (mySettings.maxVolume)) {
#if defined ROTARY_ENCODER_PRINT
    Serial.println(F("RotEncPos corrected to max"));
#endif
      RotEncPos  = mySettings.maxVolume;
    }
    else if (RotEncPos < (mySettings.minVolume)) {
#if defined ROTARY_ENCODER_PRINT
    Serial.println(F("RotEncPos corrected to min"));
#endif
      RotEncPos  = mySettings.minVolume;
    }

    if (activeModifier != NULL) {
      if (activeModifier->handleVolume() == true) {
        return;
      }
    }
    volume = RotEncPos;
    mp3.setVolume(volume);
#if defined DEBUG || defined ROTARY_ENCODER_PRINT
    Serial.print(F("volume: "));
    Serial.println(volume);
#endif
  }
}

void timerIsr() {
  encoder.service();
}
#endif

#ifdef ROTARY_SWITCH
uint8_t RotSwGetPosition () {
  float analogValue = (analogRead(ROTARY_SWITCH_PIN) * ROTARY_SWITCH_REF_VOLTAGE) / 1024.0;

  for (uint8_t x = 0; x <= ROTARY_SWITCH_POSITIONS; x++)
  {
    if (analogValue >= (ROTARY_SWITCH_STEP_MIN * x) && analogValue <= (ROTARY_SWITCH_STEP_MAX * x)) {
      return x + 1;
    }
  }
}

void RotSwloop(int TriggerTime) {

  if (RotSwCurrentPos != RotSwGetPosition()) {
    RotSwCurrentPos = RotSwGetPosition();

    if (TriggerTime > 0)
      RotSwMillis = millis();

  } else if ((millis() > (RotSwMillis + TriggerTime))  && (RotSwCurrentPos != RotSwActivePos)) {
    RotSwSet(RotSwCurrentPos);
  }
}

void RotSwSet (uint8_t RotarySwitchPosition) {
  folderSettings myRotSw = mySettings.rotarySwitchSlots[RotarySwitchPosition - 1];
  RotSwActivePos = RotarySwitchPosition;
#ifdef DEBUG
  Serial.print("RotSw active Pos ");
  Serial.println(RotSwActivePos);
#endif
  if (!(myRotSw.folder == 0 && myRotSw.mode == 0)) {
    switch (myRotSw.folder) {
      case SystemControl:
        switch (myRotSw.special) {
          case PauseSysCont: //Pause
            if (activeModifier != NULL) {
              if (activeModifier->handlePause() == true) {
#ifdef DEBUG
                Serial.println(F("Pause locked"));
#endif
                return;
              }
            }
            if (ignorePauseButton == false)
              if (isPlaying()) {
                mp3.pause();
                setstandbyTimer();
              }
            break;
          case VolumeSysCont: //Volume
            mp3.setVolume(myRotSw.special2);
            break;
          case ForwardSysCont:
            nextButton();
            break;
          case BackwardSysCont:
            previousButton();
            break;
          case ShutDownSysCont:
            shutDown();
          case RemoveModifierSysCont:
            if (activeModifier != NULL)
              RemoveModifier();
            break;
        }
        break;
      case ModifierMode:
        SetModifier (myRotSw);
        break;
      default:
        if (activeModifier != NULL) {
          if (activeModifier->handleShortCut() == true) {
#ifdef DEBUG
            Serial.println(F("Shortcut locked"));
#endif
            return;
          }
        }
        myFolder = &myRotSw;
        playFolder();
        break;
    }
  } else {
#ifdef DEBUG
    Serial.println(F("RotSw Pos not configured."));
#endif

  }
}
#endif
//////////////////////////////////////////////////////////////////////////
bool setupModifier(folderSettings * tmpFolderSettings) {

  tmpFolderSettings->folder = ModifierMode;
  tmpFolderSettings->mode = voiceMenu(10, 966, 966, false, false, 0, true);

  if (tmpFolderSettings->mode != ModifierMode) {
    if (tmpFolderSettings->mode == SleepTimerMod) {
      switch (voiceMenu(4, 960, 960)) {
        case 1: tmpFolderSettings->special = 5; break;
        case 2: tmpFolderSettings->special = 15; break;
        case 3: tmpFolderSettings->special = 30; break;
        case 4: tmpFolderSettings->special = 60; break;
      }
      tmpFolderSettings->special2 = 0x00;
    } else if (tmpFolderSettings->mode == PuzzleGameMod) {
      //Save first part?
      tmpFolderSettings->special = voiceMenu(2, 977, 933) - 1;
      tmpFolderSettings->special2 = 0x00;
    }
    else if (tmpFolderSettings->mode == QuizGameMod) {
      //Set Folder
      tmpFolderSettings->special =  voiceMenu(99, 301, 0, true, 0, 0, true);
      tmpFolderSettings->special2 = 0x00;
    }
    else if (tmpFolderSettings->mode == ButtonSmashMod) {
      //Set Folder
      tmpFolderSettings->special =  voiceMenu(99, 301, 0, true, 0, 0, true);
      //Set Volume
      tmpFolderSettings->special2 =  voiceMenu(30, 904, false, false, 0);
    }

    //Save Modifier in EEPROM?
    tmpFolderSettings->special3 = voiceMenu(2, 978, 933, false, false, 0) - 1;
    tmpFolderSettings->special4 = 0x00;
    return true;
  }
  return false;
}

bool SetModifier (folderSettings tmpFolderSettings) {
  if (activeModifier != NULL) {
    if (activeModifier->getActive() == tmpFolderSettings.mode) {
      return RemoveModifier();
    }
  }

#ifdef DEBUG
  Serial.print(F("set modifier: "));
  Serial.println(tmpFolderSettings.mode);
#endif

  if (tmpFolderSettings.mode != 0 && tmpFolderSettings.mode != AdminMenuMod) {
    if (isPlaying()) {
      mp3.playAdvertisement(260);
    }
    else {
      mp3.playMp3FolderTrack(260);
      delay(1300);
    }
  }
  delay(2000);
  switch (tmpFolderSettings.mode) {
    case 0:
    case 255:
      mfrc522.PICC_HaltA(); mfrc522.PCD_StopCrypto1(); adminMenu(true);  break;
    case 1: activeModifier = new SleepTimer(tmpFolderSettings.special); break;
    case 2: activeModifier = new FreezeDance(); break;
    case 3: activeModifier = new Locked(); break;
    case 4: activeModifier = new ToddlerMode(); break;
    case 5: activeModifier = new KindergardenMode(); break;
    case 6: activeModifier = new RepeatSingleModifier(); break;
    //    case 7: activeModifier = new FeedbackModifier(); break;
    case 8: activeModifier = new PuzzleGame(tmpFolderSettings.special); break;
    case 9: activeModifier = new QuizGame(tmpFolderSettings.special); break;
    case 10: activeModifier = new ButtonSmash(tmpFolderSettings.special, tmpFolderSettings.special2); break;
  }
  if (tmpFolderSettings.special3 == 1) {
    mySettings.savedModifier = tmpFolderSettings;
    writeSettingsToFlash();
  }
  return false;
}

bool RemoveModifier() {
  activeModifier = NULL;

  _lastTrackFinished = 0;

  mySettings.savedModifier.folder = 0;
  mySettings.savedModifier.mode = 0;
  writeSettingsToFlash();

#ifdef DEBUG
  Serial.println(F("modifier removed"));
#endif
  if (isPlaying()) {
    mp3.playAdvertisement(261);
  }
  else {
    mp3.pause();
    delay(500);
    mp3.playMp3FolderTrack(261);
    delay(1300);
  }
  return false;
}

bool setupSystemControl (folderSettings * tmpFolderSettings) {

  tmpFolderSettings->mode = ModifierMode;
  tmpFolderSettings->folder = SystemControl;
  tmpFolderSettings->special = voiceMenu(6, 952, 952);

  if (tmpFolderSettings->special != 0) {
    if (tmpFolderSettings->special == VolumeSysCont) {
      tmpFolderSettings->special2 = voiceMenu(mySettings.maxVolume - mySettings.minVolume + 1, 932, mySettings.minVolume - 1, false, false, mySettings.initVolume - mySettings.minVolume + 1) + mySettings.minVolume - 1;

    }
    return true;
  }
  return false;
}

uint8_t readAudiobookMemory (uint8_t folder, uint8_t memoryNumber) {
  uint16_t address = (folder - 1)  + ((memoryNumber - 1) * 99);
  if (memoryNumber > 0) {

#if defined DEBUG
    Serial.print(F("read memory No. "));
    Serial.print(memoryNumber);
    Serial.print(F(" of folder No. "));
    Serial.println(folder);
    Serial.print(F("at address "));
    Serial.println(address);
#endif

    return EEPROM.read(address);

  } else
    return 1;
}

void writeAudiobookMemory (uint8_t folder, uint8_t memoryNumber, uint8_t track) {
  uint16_t address = (folder - 1)  + ((memoryNumber - 1) * 99);
  if (memoryNumber > 0) {

#if defined DEBUG
    Serial.print(F("write track No. "));
    Serial.print(track);
    Serial.print(F(" of folder No. "));
    Serial.print(folder);
    Serial.print(F(" in memory No. "));
    Serial.println(memoryNumber);
#endif

    EEPROM.update(address, track);

  } else
    return;
}

//Um festzustellen ob eine Karte entfernt wurde, muss der MFRC regelmäßig ausgelesen werden
byte pollCard()
{
  const byte maxRetries = 2;

  if (!hasCard)  {
    if (mfrc522.PICC_IsNewCardPresent()) {
      if (mfrc522.PICC_ReadCardSerial()) {
#ifdef DEBUG
        Serial.println(F("ReadCardSerial finished"));
#endif
        if (readCard(&myCard))  {
          bool bSameUID = !memcmp(lastCardUid, mfrc522.uid.uidByte, 4);
          // store info about current card
          memcpy(lastCardUid, mfrc522.uid.uidByte, 4);
          lastCardWasUL = mfrc522.PICC_GetType(mfrc522.uid.sak) == MFRC522::PICC_TYPE_MIFARE_UL;

          retries = maxRetries;
          hasCard = true;
          return bSameUID ? PCS_CARD_IS_BACK : PCS_NEW_CARD;
        }
        else {//readCard war nicht erfolgreich
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
#ifdef DEBUG
          Serial.println(F("cant read card"));
#endif
        }
      }
    }
    return PCS_NO_CHANGE;
  }
  else // hasCard
  {
    // perform a dummy read command just to see whether the card is in range
    byte buffer[18];
    byte size = sizeof(buffer);

    if (mfrc522.MIFARE_Read(lastCardWasUL ? 8 : blockAddr, buffer, &size) != MFRC522::STATUS_OK) {
      if (retries > 0)
        retries--;
      else {
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        hasCard = false;
        return PCS_CARD_GONE;
      }
    }
    else
      retries = maxRetries;
  }
  return PCS_NO_CHANGE;
}

void handleCardReader()
{
  bool tmpStopWhenCardAway = true;

  // poll card only every 100ms
  static unsigned long lastCardPoll = 0;
  unsigned long now = millis();

  if (static_cast<unsigned long>(now - lastCardPoll) > 100)  {
    if (activeModifier != NULL)
      tmpStopWhenCardAway = mySettings.stopWhenCardAway && !activeModifier->handleStopWhenCardAway();
    else
      tmpStopWhenCardAway = mySettings.stopWhenCardAway;

    lastCardPoll = now;
    switch (pollCard()) {
      case PCS_NEW_CARD:
#ifdef DEBUG
        Serial.println(F("new card"));
#endif
        onNewCard();
        break;
      case PCS_CARD_GONE:
#ifdef DEBUG
        Serial.println(F("card gone"));
#endif
        if (tmpStopWhenCardAway) {
          knownCard = false;
          mp3.pause();
          setstandbyTimer();
        }
        break;
      case PCS_CARD_IS_BACK:
#ifdef DEBUG
        Serial.println(F("same card"));
#endif
        if (tmpStopWhenCardAway) {

          //nur weiterspielen wenn vorher nicht konfiguriert wurde
          if (!forgetLastCard) {
            knownCard = true;
            mp3.start();
            disablestandbyTimer();
          }
          else
            onNewCard();
        } else {
          onNewCard();
        }
        break;
    }
  }
}

void onNewCard() {
  forgetLastCard = false;
  // make random a little bit more "random"
  //randomSeed(millis() + random(1000));
  if (myCard.cookie == cardCookie && myCard.nfcFolderSettings.folder != 0 && myCard.nfcFolderSettings.mode != 0) {
    playFolder();
  }

  // Neue Karte konfigurieren
  else if (myCard.cookie != cardCookie) {
    mp3.playMp3FolderTrack(300);
    waitForTrackToFinish();
    setupCard();
  }
}

/// Funktionen für den Standby Timer (z.B. über Pololu-Switch oder Mosfet)

void setstandbyTimer() {
#ifdef DEBUG
  Serial.println(F("set standby timer"));
#endif
#ifdef SPEAKER_SWITCH
  digitalWrite(SpeakerOnPin, LOW);
  delay(100);
#endif
  if (mySettings.standbyTimer != 0)
    sleepAtMillis = millis() + (mySettings.standbyTimer * 60 * 1000);
  else
    sleepAtMillis = 0;
#ifdef DEBUG
  Serial.print(F("milis: "));
  Serial.println(sleepAtMillis);
#endif
}
//////////////////////////////////////////////////////////////////////////
void disablestandbyTimer() {
#ifdef DEBUG
  Serial.println(F("disable standby timer"));
#endif
#ifdef SPEAKER_SWITCH
  digitalWrite(SpeakerOnPin, HIGH);
  delay(100);
#endif
  sleepAtMillis = 0;
}
//////////////////////////////////////////////////////////////////////////
void checkStandbyAtMillis() {
  if (sleepAtMillis != 0 && millis() > sleepAtMillis) {
#ifdef DEBUG
    Serial.println(F("standby active"));
#endif
    // enter sleep state
    // http://discourse.voss.earth/t/intenso-s10000-powerbank-automatische-abschaltung-software-only/805
    // powerdown to 27mA (powerbank switches off after 30-60s)
    shutDown();
  }
}
//////////////////////////////////////////////////////////////////////////
void shutDown() {
  if (activeModifier != NULL) {
    if (activeModifier->handleShutDown() == true) {
#ifdef DEBUG
      Serial.println(F("Shut down locked"));
#endif
      return;
    }
  }

#ifdef DEBUG
  Serial.println("Shut Down");
#endif

  mp3.pause();

#ifdef SPEAKER_SWITCH
  digitalWrite(SpeakerOnPin, HIGH);
  delay(100);
#endif

#ifdef POWER_ON_LED
  digitalWrite(PowerOnLEDPin, LOW);
#endif

  delay(500);

#ifdef STARTUP_SOUND
  if (volume > mySettings.initVolume)
    volume = mySettings.initVolume;

  mp3.setVolume(volume);
  delay(500);

  knownCard = false;
  mp3.playMp3FolderTrack(265);
  waitForTrackToFinish();
#endif

#ifdef SPEAKER_SWITCH
  digitalWrite(SpeakerOnPin, LOW);
  delay(100);
#endif

  digitalWrite(shutdownPin, HIGH);

  mfrc522.PCD_AntennaOff();
  mfrc522.PCD_SoftPowerDown();
  mp3.sleep();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();  // Disable interrupts
  sleep_mode();
}

//////////////////////////////////////////////////////////////////////////
#if defined FADING_LED
// fade in/out status led while beeing idle, during playback set to full brightness
void fadeStatusLed(bool isPlaying) {
  static bool statusLedDirection = false;
  static int16_t statusLedValue = 255;
  static uint64_t statusLedOldMillis;
  static int16_t statusLedDeltaValuePause = 100;
  static int16_t statusLedDeltaValuePlay = 10;
  static int16_t statusLedDeltaValue = 10;

  if (isPlaying) {
    statusLedDeltaValue = statusLedDeltaValuePlay;
  }
  else {
    statusLedDeltaValue = statusLedDeltaValuePause;
  }
  uint64_t statusLedNewMillis = millis();
  if (statusLedNewMillis - statusLedOldMillis >= 100) {
    statusLedOldMillis = statusLedNewMillis;
    if (statusLedDirection) {
      statusLedValue += statusLedDeltaValue;
      if (statusLedValue >= 255) {
        statusLedValue = 255;
        statusLedDirection = !statusLedDirection;
      }
    }
    else {
      statusLedValue -= statusLedDeltaValue;
      if (statusLedValue <= 0) {
        statusLedValue = 0;
        statusLedDirection = !statusLedDirection;
      }
    }
    analogWrite(PowerOnLEDPin, statusLedValue);
  }
}
/////////////////////
#endif
