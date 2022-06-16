#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"

#include "Adafruit_SSD1306.h"
#include "SoftwareSerial.h"

/**
 * OLED Screen Constants
 */
#define SCREEN_WIDTH    128 // OLED display width, in pixels
#define SCREEN_HEIGHT   32 // OLED display height, in pixels
#define OLED_RESET      4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS  0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

/**
 * Screen Display
 */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/**
 * Serial Port
 */
#define PIN_RX 9
#define PIN_TX 10

/**
 * Commands Static Values
 */
#define CMD_END      0x14F
#define CMD_RECEIVE   0x180

/**
 * Registers to manage status
 */
bool cnn_is_turned_on = false;
bool cdc_initialized = false;
bool cdc_is_no_cd = false;
bool cdc_is_playing = false;

char const *executed_function = "";

/**
 * Playing music status
 */
unsigned int playing_total = 0;
unsigned int playing_minutes = 0;
unsigned int playing_seconds = 0;
unsigned int playing_disc = 0;
unsigned int playing_song = 0;
unsigned long playing_time = 0;
unsigned long playing_time_last_sent = 0;

/**
 * Serial communication registers
 */
unsigned int bitDuration = 200;
unsigned last_command;
unsigned sent_command;
bool sending_echo = false;
bool sending_commands = false;
bool receiving_commands = false;

/**
 * Setup Connect NAV+ Serial Connection
 */
SoftwareSerial serial = SoftwareSerial(PIN_RX, PIN_TX, true);

/**
 * @name set_executed_function
 *  Set executed function
 *
 * @param command
 */
void set_executed_function(const char *command) {
    executed_function = command;
}

/**
 * @name initialize_display
 * initialize_display
 *
 * @return void
 */
void initialize_display() {
    set_executed_function("initialize_display");
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    delay(2000); // Pause for 2 seconds

    // Clear the buffer
    display.clearDisplay();
}

/**
 * Set SoftwareSerial bound rate
 * @param bound
 */
void set_serial_bound(int bound) {
    set_executed_function("set_serial_bound");
    serial.begin(bound);
    bitDuration = (bound == 4800) ? 200 : 100;
}

/**
 * @name rx
 * Read Connect NAV+ Data from serial port
 *
 * @return
 */
unsigned rx() {
    unsigned data = NULL;
    receiving_commands = true;
    /**
     * @todo: Remove while to update the prevent stuck statuses
     */
    do {
        if (serial.available()) {
            data = serial.read();
            // Fix 8bits to 9 bits conversion
            switch (0x1FF & data) {
                case 0x80: {
                    data = CMD_RECEIVE;
                    break;
                }
                case 0x4F: {
                    data = CMD_END;
                    break;
                }
            }
        }
    } while (!data);

    if (data != CMD_END) {
        last_command = data;
    }
    receiving_commands = false;
    return data;
};

void send_bit(bool value, int duration = 1) {
    set_executed_function("send_bit");
    digitalWrite(PIN_TX, (value) ? HIGH : LOW);
    delayMicroseconds(bitDuration * duration);
}

/**
   Transmit value using serial connection
*/
void tx(unsigned value) {
    set_executed_function("tx");
    sending_commands = true;

    send_bit(0, 4);
    send_bit(1);

    if (value) {
        for (int i = 0; i < 9; i++) {
            send_bit(bitRead(~value, i));
        }
    } else {
        sent_command = 0x00;
        send_bit(1, 9);
    }
    send_bit(0, 4);

    if (value == CMD_END)
        delay(3);
    sending_commands = false;
}


/**
 * Send a value and wait for the answer
*/
unsigned echo_command(unsigned value) {
    sending_echo = true;
    set_executed_function("echo_command");
    tx(value);
    if (value != CMD_END) {
        sent_command = value;
    }
    if (value) {
        return rx();
    }
    return NULL;
}


/**
 * @name transmit_commands
 * Transmit a list of values
 *
 * @param list List of commands
 * @param size
 * @param first
 */
void transmit_commands(int *list, int size, int first = 0) {
    set_executed_function("transmit_commands");

    for (int i = first; i < first + size; i++) {
        echo_command(list[i]);
        delay((list[i] == CMD_END) ? 30 : 3);
    }
}

/**
   Send commands to show No Disc
*/
void send_no_disc() {
    set_executed_function("send_no_disc");
    cdc_is_no_cd = true;
    int no_disc_commands[] = {
            0x10E, 0x009, 0x007, CMD_END,
            0x103, 0x020, 0x00A, 0x010, 0x000, CMD_END,
            0x10C, 0x001, CMD_END,
    };
    transmit_commands(no_disc_commands, 4, 0);
    delay(30);
    transmit_commands(no_disc_commands, 6, 4);
    delay(30);
    transmit_commands(no_disc_commands, 3, 10);
}


int set_disc_ready(int disc, int songs) {
    set_executed_function("set_disc_ready");
    int discReady[] = {
            // Set disc position is being verified
            0x101, disc, 0x001, CMD_END,
            0x103, 0x020, 0x009, 0x020, 0x000, CMD_END,
            // Set disc as readable
            0x101, disc, 0x001, CMD_END,
            // Set title of Disc
            0x010B, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0047, CMD_END,
            // Songs quantities
            0x010D, disc, songs, 0x0047, 0x0011, CMD_END,
    };
    transmit_commands(discReady, 4);
    delay(30);
    transmit_commands(discReady, 6, 4);
    delay(30);
    transmit_commands(discReady, 10, 14);
    delay(30);
    transmit_commands(discReady, 6, 24);
    delay(30);
    transmit_commands(discReady, 4, 10);
    delay(30);
    transmit_commands(discReady, 6, 4);
}

void reply_command(unsigned data) {
    set_executed_function("reply_command");
    switch (data) {
        case 0xA9: {
            tx(data);
            tx(0x00);
            break;
        }
        case CMD_END: {
            break;
        }
        default: {
            tx(data);
            reply_command(rx());
        }
    }
}

void send_play_disc(int disc, int song, int duration) {
    set_executed_function("send_play_disc");
    cdc_is_playing = true;

    int execution[]{
            0x103, 0x020, 0x009, 0x020, 0x000, CMD_END,
            0x101, disc, song, CMD_END,
    };

    playing_total = duration;
    transmit_commands(execution, 10);
    delay(30);
}

void setup() {
    set_executed_function("setup");
    // Define pin modes for tx, rx, led pins:
    pinMode(PIN_RX, INPUT);
    pinMode(PIN_TX, OUTPUT);
    Serial.begin(9600);
    initialize_display();


    // Set the data rate for the SoftwareSerial port
    set_serial_bound(4800);

    // Define the TX line to 0
    send_bit(0);
}


void draw_command_screen() {
    display.clearDisplay();
    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font

    bool stepsStatus[] = {
            cnn_is_turned_on,
            cdc_initialized,
            cdc_is_no_cd,
    };

    for (bool &stepsStat: stepsStatus) {
        display.write(stepsStat ? 0xDB : 0x20);
    }

    if (sending_commands || receiving_commands) {
        digitalWrite(LED_BUILTIN, HIGH);
    }

    display.setCursor(0, 9);

    //  display.setCursor(60, 0);
    if (last_command) {
        String description = "";

        display.setTextSize(1);
        switch (last_command) {
            case 0xAD: {
                description = "Boot";
                break;
            }
            case 0x21: {
                description = "Sending command";
            }
        }
        display.println(description);
        display.print(F("0x"));
        display.print(last_command, HEX);
        display.print(F(" 0x"));
        display.println(sent_command, HEX);
    }

    display.display();
}

/**
 * draw_song_status
 * Display dashboard on Oled Screen
 */
void draw_song_status() {
    display.setTextSize(1);          // Normal 1:1 pixel scale
    display.setCursor(0, 24);     // Start at top-left corner
    display.cp437(true);             // Use full 256 char 'Code Page 437' font

    if (cdc_is_playing) {
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

void set_song_status() {
    set_executed_function("sendSongTime");
    bool finished = false;

    unsigned long currentMillis = millis();
    unsigned long playing_time_ms = currentMillis - playing_time;

    if (playing_time_ms >= 1000) {
        div_t divisionOfTime = div(playing_time_ms / 1000, 60);
        playing_minutes = (int) divisionOfTime.quot;
        playing_seconds = (int) ((playing_time_ms / 1000) - (playing_minutes * 60));

        if (playing_time_ms - playing_time_last_sent >= 1000) {
            playing_time_last_sent = playing_time_ms;
            int stats[] = {0x109, (int) playing_minutes, (int) playing_seconds, CMD_END};
            transmit_commands(stats, 4);
            Serial.print(playing_minutes);
            Serial.print(":");
            Serial.println(playing_seconds);
        }
        draw_song_status();
    }
}

/**
 * @name loop
 */
void loop() {
//    draw_command_screen();
    if (cdc_is_playing) {
        while (!playing_total) {
            set_song_status();
        }
    }

    unsigned data = rx();

    if (data == CMD_RECEIVE) {
        cnn_is_turned_on = true;
        reply_command(data);
    }

    switch (last_command) {
        case 0x21: {
//            delay(1000);
            send_no_disc();
            cdc_initialized = true;
            break;
        }
        case 0xA5: {
            delay(30);
            set_disc_ready(1, 5); // Testado
            delay(1000);
            cdc_is_playing = true;
            break;
        }
        default: {
            if (cdc_is_playing) {
                send_play_disc(1, 1, 300);
            }
        }
    }
}