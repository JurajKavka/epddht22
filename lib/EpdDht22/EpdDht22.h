#include <Arduino.h>
#include <DHT.h>
#include "CircularArray.h"
#include <GxEPD2_AVR_BW.h>

#define DHT_TYPE DHT22

// weather font
#define BATTERY_100 '!'
#define BATTERY_75 '"'
#define BATTERY_50 '#'
#define BATTERY_25 '$'
#define BATTERY_0 '&'
#define THERMOMETER_100 '`'
#define WATER_DROP '|'
#define DEGREE_SIGN '~'

const uint8_t FIVE_MIN_BUFFER_SIZE = 4;
const uint8_t TWENTY_MIN_BUFFER_SIZE = 6;
const uint8_t TWO_HOURS_BUFFER_SIZE = 12;


enum Envinroment {
    development,
    test,
    production
};


struct Settings {
    uint8_t pinDht22;
    uint8_t pinTransistorSwitch;
    uint8_t envin;
};


struct Dht22Data {
    float temperature;
    float humidity;
};


struct MinMax {
    float min;
    float max;
};


struct Range {
    float down;
    float up;
    float size;
};


class EpdDht22 {
    private:
        Settings *_settings;
        DHT *_dht22;
        GxEPD2_AVR_BW *_display;

        // 5 minutes buffer
        Dht22Data _fmb[FIVE_MIN_BUFFER_SIZE];
        CircularArray<Dht22Data> *_fiveMinuteBuffer;

        // 20 minutes buffer
        Dht22Data _tmb[TWENTY_MIN_BUFFER_SIZE];
        CircularArray<Dht22Data> *_twentyMinuteBuffer;

        // 2 hours buffer (or 24 hours history)
        Dht22Data _2hb[TWO_HOURS_BUFFER_SIZE];
        CircularArray<Dht22Data> *_twoHourBuffer;

        // TODO: get rid of this variable
        Range _range;

        void _setPinsLow();
        void _debugDataBuffer();
        void _debugHistoryBuffer();

        Dht22Data _getAverageValues(CircularArray<Dht22Data> *buffer);

        // graph functions
        void _writeLine(uint16_t, uint16_t, uint16_t, uint16_t);
        void _drawBar(float value, uint16_t xPos);
        void _printData(Dht22Data *data);
        void _printHistory();
        void _printVcc(long vcc);
    public:
        EpdDht22(Settings *settings);
        void powerUp();
        void powerDown();
        Dht22Data readDht22();
        Dht22Data twentyMinuteAverage();
        Dht22Data twoHourAverage();
        void printScreen();
        long readVcc();
};
