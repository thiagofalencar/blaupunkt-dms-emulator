#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"

#include "Adafruit_SSD1306.h"
#include "SoftwareSerial.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define rxPin 9
#define txPin 10

#define cmd_end 0x14F


bool initialized  = false;
bool handshake    = false;
bool noCD         = false;
bool playing      = false;

char const *executing = "";

int playing_total   = 0;
int playing_minutes = 0;
int playing_seconds = 0;
int playing_disc    = 0;
int playing_song    = 0;

unsigned long playing_time = 0;
unsigned long playing_time_last_sent = 0;

int bitDuration = 200;
unsigned lastCommand = NULL;
unsigned sentCommand = NULL;

// Set up Connect NAV+ Serial Connection
SoftwareSerial serial =  SoftwareSerial(rxPin, txPin, true);

void initializeDisplay() {
    executing = "initializeDisplay";
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    delay(2000); // Pause for 2 seconds

    // Clear the buffer
    display.clearDisplay();
}

void setBound(int bound) {
    executing = "setBound";
    serial.begin(bound);
    bitDuration = (bound == 4800) ? 200 : 100;
}

// Read Connect NAV+ Data from serial port
unsigned rx() {
    unsigned data = NULL;
    do {
        if (serial.available()) {
            data = serial.read();
            // Fix 8bits to 9 bits conversion
            switch (0x1FF & data) {
                case 0x80: {
                    data = 0x180;
                    break;
                }
                case 0x4F: {
                    data = cmd_end;
                    break;
                }
            }
        }
    } while (!data);

    if (data != cmd_end) {
        lastCommand = data;
    }
    return data;
};

void sendBit(bool value, int duration = 1) {
    executing = "sendBit";
    digitalWrite(txPin, (value) ? HIGH : LOW);
    delayMicroseconds(bitDuration * duration);
}

/**
   Transmit value using serial connection
*/
void tx(unsigned value) {
    executing = "tx";

    sendBit(0, 4);
    sendBit(1);

    if (value) {
        for (int i = 0; i < 9; i++) {
            sendBit(bitRead(~value, i));
        }
    } else {
        sentCommand = 0x00;
        sendBit(1, 9);
    }
    sendBit(0, 4);

    if (value == cmd_end)
        delay(3);
}


/**
   Send a value and wait for the answer
   @returns bool
*/
void echo(unsigned value) {
    executing = "echo";
    tx(value);
    if (value != cmd_end) {
        sentCommand = value;
    }
    if (value) {
        rx();
    }
}

/**
   Transmit a list of values
*/
void txList(int *list, int size, int first = 0) {
    executing = "txList";

    for (int i = first; i < first + size; i++) {
        echo(list[i]);
        delay((list[i] == cmd_end) ? 30 : 3);
    }
}

/**
   Send commands to show No Disc
*/
void setNoCD() {
    executing = "setNoCD";
    noCD = true;
    int noCDCmds[] = {
            0x10E, 0x009, 0x007, cmd_end,
            0x103, 0x020, 0x00A, 0x010, 0x000, cmd_end,
            0x10C, 0x001, cmd_end,
    };
    txList(noCDCmds, 13);
}


void setDiscReady(int disc, int songs) {
    executing = "setDiscReady";
    int discReady[] = {
            // Set disc position is being verified
            0x101,  disc, 0x001, cmd_end,
            0x103, 0x020, 0x009, 0x020, 0x000, cmd_end,
            // Set disc as readable
            0x101, disc, 0x001, cmd_end,
            // Set title of Disc
            0x010B, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0047, 0x014F,
            // Songs quantities
            0x010D, disc, songs, 0x0047, 0x0011, 0x014F,
    };
    txList(discReady, 4);
    delay(30);
    txList(discReady, 6, 4);
    delay(30);
    txList(discReady, 10, 14);
    delay(30);
    txList(discReady, 6, 24);
    delay(30);
    txList(discReady, 4, 10);
    delay(30);
    txList(discReady, 6, 4);
}

void replyCommand(unsigned data) {
    executing = "replyCommand";
    switch (data) {
        case 0xA9: {
            tx(data);
            tx(0x00);
            break;
        }
        case cmd_end: {
            break;
        }
        default: {
            tx(data);
            replyCommand(rx());
        }
    }
}

void updateSongTime() {
    executing = "sendSongTime";
    bool finished = false;

    unsigned long currentMillis = millis();
    unsigned long playing_time_ms = currentMillis - playing_time;

    if (playing_time_ms >= 1000) {
        div_t divisionOfTime = div(playing_time_ms / 1000, 60);
        playing_minutes = divisionOfTime.quot;
        playing_seconds = (playing_time_ms / 1000) - ((int) playing_minutes * 60);

        if (playing_time_ms - playing_time_last_sent >= 1000) {
            playing_time_last_sent = playing_time_ms;
            int stats[] = { 0x109, playing_minutes, playing_seconds, cmd_end };
            txList(stats, 4);
        }
    }
}


void playDisc(int disc, int song, int duration) {
    executing = "playDisc";
    playing = true;
    int execution[] {
            0x103, 0x020, 0x009, 0x020, 0x000, cmd_end,
            0x101, disc, song, cmd_end,
    };
    playing_total = duration;
    txList(execution, 10);
    delay(30);
}

void setup()  {
    executing = "setup";
    // Define pin modes for tx, rx, led pins:
    pinMode(rxPin, INPUT);
    pinMode(txPin, OUTPUT);
    Serial.begin(9600);
    initializeDisplay();


    // Set the data rate for the SoftwareSerial port
    setBound(4800);

    // Define the TX line to 0
    sendBit(0);
}


void drawListOfCommands() {
    display.clearDisplay();

    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font

    char steps[][8] =  { " Init", " Hand", " No Disc" };
    char stepsStatus[] = { initialized, handshake, noCD };

    //  for (int i = 0; i < 3; i++) {
    //    display.write((stepsStatus[i]) ? 0x1A : ' ');
    //    display.write(steps[i]);
    //    display.write('\n');
    //  }

    //  display.setCursor(60, 0);
    if (lastCommand) {
        display.print(F("0x"));
        display.print(lastCommand, HEX);
        display.print(F(" 0x"));
        display.println(sentCommand, HEX);
        String desc = "";
        display.setTextSize(1);
        switch (lastCommand) {
            case 0xAD: {
                desc = "Boot";
                break;
            }
            case 0x21: {
                desc = "Sending command";
            }
        }
        display.println(desc);
    }


    //  // Not all the characters will fit on the display. This is normal.
    //  // Library will draw what it can and the rest will be clipped.
    //  for(int16_t i=0; i<256; i++) {
    //    if(i == '\n') display.write(' ');
    //    else          display.write(i);
    //  }

    display.display();
}

void drawPlayTime() {

    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setCursor(0, 24);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font

    if (playing) {
        display.setCursor(8, 24);
        display.write(0x10);
        display.setCursor(16, 24);
        display.write(0x10);
    }

    display.setCursor(24, 24);
    display.print(" Disc ");
    display.print(playing_disc, DEC);
    display.print("-");

    if (playing_song < 10) {
        display.print(0, DEC);
    }
    display.print(playing_song, DEC);
    display.print(" ");

    if (playing_minutes < 10) {
        display.print(0, DEC);
    }
    display.print(playing_minutes, DEC);

    display.print(":");
    if (playing_seconds < 10) {
        display.print(0, DEC);
    }
    display.print(playing_seconds, DEC);

    display.display();
}

void loop() {
    drawListOfCommands();
    if (playing) {
        updateSongTime();
        drawPlayTime();
    }

    unsigned data = rx();

    if (data == 0x180) {
        initialized = true;
        replyCommand(data);
    }

    switch (lastCommand) {
        case 0x21: {
            handshake = true;
            delay(5000);
            setNoCD();
            break;
        }
        case 0xA5: {
            if (lastCommand == 0xA5) {
                delay(30);
                setDiscReady(1, 5); // Testado
                delay(1000);
                playing = true;
            }
            break;
        }
        default: {
            if (playing) {
                playDisc(1, 1, 300);
            }
        }
    }
}