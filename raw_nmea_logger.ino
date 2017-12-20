/*
    Raw NMEA Logger for Arduino

    Copyright (c) 2017 by Tak Yanagida (OnTarget Inc.)
    This software is released under the MIT License.
*/

#include <SPI.h>
#include <SD.h>

// Pin configurations
const uint8_t CARD_SELECT = 4;
const uint8_t SDCARD_LED =  8;
const uint8_t RED_LED = 13;
const uint8_t VBAT_PIN = A7;

// Other configurations
const float LIPO_LOWBAT_THRESHOLD = 3.55;
const unsigned long LOWBAT_CHECK_INTERVAL = 60000; // in millis
const unsigned long LOWBAT_BLINK_INTERVAL = 500;
const unsigned long SD_FLUSH_INTERVAL = 1000;
const unsigned long STATUS_REPORT_INTERVAL = 10000;
const char* const FILE_PREFIX = "AA"; // you can change this to distinguish multiple loggers
const char* const FILE_EXT = ".txt";
const char* const LASTNUM_FILE = "LASTNUM.txt";
const int FILENUM_LEN = 6;
const char* const INITIAL_NUM = "000000";
const char* const DEFAULT_NUM = "999999";

// global variables
File logfile;
unsigned long num_lines;
struct {
    bool lowbat = false;
} global_status;

// blink out an error code
void error(uint8_t errno) {
    while (1) {
        uint8_t i;
        for (i = 0; i < errno; i++) {
            digitalWrite(RED_LED, HIGH);
            delay(100);
            digitalWrite(RED_LED, LOW);
            delay(100);
        }
        for (i = errno; i < 10; i++) {
            delay(200);
        }
    }
}

void read_nmea_and_log() {
    static unsigned long last_sd_flush_millis = 0;
    unsigned long current_millis;

    while (Serial1.available()) {
        char c = Serial1.read();
        logfile.print(c);
        if (c == '\n') {
            num_lines++;
            Serial.print("LINES: ");
            Serial.println(num_lines);

            current_millis = millis();
            if (current_millis < last_sd_flush_millis + SD_FLUSH_INTERVAL) {
                continue;
            }
            digitalWrite(SDCARD_LED, HIGH);
            logfile.flush();
            last_sd_flush_millis = current_millis;
            digitalWrite(SDCARD_LED, LOW);
        }
    }
}

void check_battery() {
    float vbat = analogRead(VBAT_PIN) * 2 * 3.3 / 1024;
    Serial.print("VBat: ");
    Serial.println(vbat);
    if (vbat < LIPO_LOWBAT_THRESHOLD) {
        global_status.lowbat = true;
    }
}

void show_global_status() {
    static int red_led_state = LOW;
    static unsigned long last_blink_millis = 0;
    static unsigned long last_status_reported_millis = 0;
    unsigned long current_millis = millis();

    if (current_millis > last_status_reported_millis + STATUS_REPORT_INTERVAL) {
        Serial.print("lowbat: ");
        Serial.println(global_status.lowbat);
        last_status_reported_millis = current_millis;
    }
    if (global_status.lowbat) {
        if (current_millis > last_blink_millis + LOWBAT_BLINK_INTERVAL) {
            if (red_led_state == LOW) {
                red_led_state = HIGH;
            } else {
                red_led_state = LOW;
            }
            digitalWrite(RED_LED, red_led_state);
            last_blink_millis = current_millis;
        }
    }
}

void gen_filename(char* new_filename)
{
    File lastnum_file;
    char new_filenum[FILENUM_LEN];

    lastnum_file = SD.open(LASTNUM_FILE, FILE_READ);
    if (!lastnum_file) {
        strcpy(new_filenum, INITIAL_NUM);
        goto save_filenum;
    }

    lastnum_file.seek(0);
    lastnum_file.read(new_filenum, FILENUM_LEN);
    for (int i = 0; i < FILENUM_LEN; i++) {
        if (!isDigit(new_filenum[i])) {
            strcpy(new_filenum, DEFAULT_NUM);
            lastnum_file.close();
            goto save_filenum;
        }
    }
    lastnum_file.close();

    // increment new_filenum only when successfully read from file
    {
        int i = FILENUM_LEN - 1;
        while (++new_filenum[i] > '9') {
            new_filenum[i] = '0';
            if (--i < 0) {
                break;
            }
        }
    }

save_filenum:
    lastnum_file = SD.open(LASTNUM_FILE, FILE_WRITE);
    lastnum_file.seek(0);
    lastnum_file.print(new_filenum);
    lastnum_file.close();

    strcpy(new_filename, FILE_PREFIX);
    strcat(new_filename, new_filenum);
    strcat(new_filename, FILE_EXT);
    return;
}

void setup() {
    // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
    Serial.begin(115200);
    Serial.println("\r\nGPS logger start");
    pinMode(RED_LED, OUTPUT);

    // initialize GPS module
    Serial1.begin(9600);
    num_lines = 0;

    // see if the card is present and can be initialized
    if (!SD.begin(CARD_SELECT)) {
        Serial.println("Card init. failed!");
        error(2);
    }
    char filename[15];
    gen_filename(filename);
    logfile = SD.open(filename, FILE_WRITE);
    if (!logfile) {
        Serial.print("Couldn't create ");
        Serial.println(filename);
        error(3);
    }
    Serial.print("Writing to ");
    Serial.println(filename);

    // LED configuration
    pinMode(RED_LED, OUTPUT);
    pinMode(SDCARD_LED, OUTPUT);

    Serial.println("Ready!");
    digitalWrite(RED_LED, LOW);

    // initialize MTK3339
    const char* nmea_init_sentences[] = {
        "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28", // change NMEA sentence frequency
        "$PMTK401*37", // PMTK_API_Q_DGPS_ENABLED
        "$PMTK413*34", // PMTK_API_Q_SBAS_ENABLED
        "$PMTK414*33", // PMTK_API_Q_NMEA_OUTPUT
        "$PMTK419*3E", // PMTK_API_Q_SBAS_MODE
        "$PMTK605*31", // PMTK_Q_RELEASE
        "$PMTK607*33", // PMTK_Q_EPO_INFO
        "$PMTK447*35", // PMTK_Q_Nav_Threshold
        "$PMTK430*35", // PMTK_API_Q_DATUM
        NULL
    };

    for (int i = 0; nmea_init_sentences[i]; i++) {
        Serial1.println(nmea_init_sentences[i]);
        read_nmea_and_log();
    }
}

void loop() {
    static unsigned long last_lowbat_check_millis = 0;

    read_nmea_and_log();
    show_global_status();

    unsigned long current_millis = millis();
    if (millis() > last_lowbat_check_millis + LOWBAT_CHECK_INTERVAL) {
        check_battery();
        last_lowbat_check_millis = current_millis;
    }
}
