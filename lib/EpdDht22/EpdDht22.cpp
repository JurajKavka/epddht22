#include "EpdDht22.h"
#include "fonts/Georgia-weather18pt7b.h"
#include "Fonts/TomThumb.h"
#include <math.h>

// delay after power on in miliseconds
#define SWITCH_POWER_DELAY 500

// pause between DHT22 readouts
#define READ_DHT22_PAUSE 2500

// left margin
#define MARGIN_LEFT 30
#define TEMPERATURES_TOP 50
#define LINE 50

const uint16_t GRAPH_X = 8;
const uint16_t GRAPH_Y = 128;
const uint16_t GRAPH_WIDTH = 190;
const uint16_t GRAPH_HEIGHT = 70;

const uint16_t X_AXIS_X = (GRAPH_X + 25);
const uint16_t X_AXIS_Y = (GRAPH_Y + GRAPH_HEIGHT - 5);
const uint16_t X_AXIS_WIDTH = (GRAPH_WIDTH);

const uint16_t Y_AXIS_X = (X_AXIS_X);
const uint16_t Y_AXIS_Y = (X_AXIS_Y);
const uint16_t Y_AXIS_HEIGHT = (GRAPH_HEIGHT);


EpdDht22::EpdDht22(Settings *settings){
    _settings = settings;

    // initialize devices
    _dht22 = new DHT(_settings->pinDht22, DHT_TYPE);
    _display = new GxEPD2_AVR_BW(GxEPD2::GDEP015OC1, /*CS=*/ SS, /*DC=*/ 8,
                                 /*RST=*/ 9, /*BUSY=*/ 7);
    // initialize buffers
    _fiveMinuteBuffer = new CircularArray<Dht22Data>(_fmb, 
                                                     FIVE_MIN_BUFFER_SIZE);
    _twentyMinuteBuffer = new CircularArray<Dht22Data>(_tmb, 
                                                       TWENTY_MIN_BUFFER_SIZE);
    _twoHourBuffer = new CircularArray<Dht22Data>(_2hb, 
                                                  TWO_HOURS_BUFFER_SIZE);

    // set all the pins low for better power saving
    _setPinsLow();

    // start up DHT22 sensor
    _dht22->begin();

}


long EpdDht22::readVcc() { 
    long result; // Read 1.1V reference against AVcc 
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); 
    delay(2); // Wait for Vref to settle 
    ADCSRA |= _BV(ADSC); // Convert 
    while (bit_is_set(ADCSRA,ADSC)); 
    result = ADCL; 
    result |= ADCH<<8; 
    result = 1126400L / result; // Back-calculate AVcc in mV 
    return result; 
}


Dht22Data EpdDht22::_getAverageValues(CircularArray<Dht22Data> *buffer){
    Dht22Data sum = { 0., 0. };
    Dht22Data average = { 0., 0. };

    for(uint16_t i=0; i<buffer->size(); i++){
        Dht22Data *_tmp = buffer->get(i);
        sum.temperature += _tmp->temperature;
        sum.humidity += _tmp->humidity;
    }

    average.temperature = sum.temperature / (buffer->size());
    average.humidity = sum.humidity / (buffer->size());
    return average;
}


void EpdDht22::_setPinsLow(){
    for (byte i=0; i<20; i++) {
        pinMode(i, INPUT_PULLUP);
        digitalWrite(i, LOW);
    }
}


void EpdDht22::_debugDataBuffer(){
    for(uint16_t i=0; i<_fiveMinuteBuffer->size(); i++){
        Serial.print(_fiveMinuteBuffer->get(i)->temperature);
        if(i != _fiveMinuteBuffer->size() - 1) Serial.print(", ");
    }
    Serial.println();

}

void EpdDht22::_debugHistoryBuffer(){
    for(uint16_t i=0; i<_twoHourBuffer->size(); i++){
        Serial.print(_twoHourBuffer->get(i)->temperature);
        if(i != _twoHourBuffer->size() - 1) Serial.print(", ");
    }
    Serial.println();
}


void EpdDht22::_writeLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
    _display->writeLine(x0, y0, x1, y1, GxEPD_BLACK);
}


void EpdDht22::_drawBar(float value, uint16_t xPos){
    // compute height of bar
    uint16_t height = (
        round(((value - _range.down) / _range.size) * GRAPH_HEIGHT)
    );
    // print bar
    _display->drawRect((xPos - 5), (Y_AXIS_Y - height), 10, height, 
                       GxEPD_BLACK);
}


void EpdDht22::powerUp(){
    pinMode(_settings->pinTransistorSwitch, OUTPUT);
    digitalWrite(_settings->pinTransistorSwitch, HIGH);
    delay(SWITCH_POWER_DELAY);
    _display->init();
}


void EpdDht22::powerDown(){
    delay(SWITCH_POWER_DELAY);
    digitalWrite(_settings->pinTransistorSwitch, LOW);
    pinMode(_settings->pinTransistorSwitch, INPUT);
    SPI.end();
    _setPinsLow();
}


Dht22Data EpdDht22::readDht22(){

    // read DHT22 to void to clean buffer
    _dht22->readTemperature();
    _dht22->readHumidity();

    delay(READ_DHT22_PAUSE);

    Dht22Data _tmp = {
        _dht22->readTemperature(),
        _dht22->readHumidity()
    };

    _fiveMinuteBuffer->push(_tmp);

    return _tmp;
}


Dht22Data EpdDht22::twentyMinuteAverage(){
    Dht22Data _avg20min = _getAverageValues(_fiveMinuteBuffer);
    _twentyMinuteBuffer->push(_avg20min);
    return _avg20min;
}


Dht22Data EpdDht22::twoHourAverage(){
    Dht22Data _avg2h = _getAverageValues(_twentyMinuteBuffer);
    _twoHourBuffer->push(_avg2h);
    return _avg2h;
}


void EpdDht22::_printHistory(){

#ifdef DBG
    _debugHistoryBuffer();
#endif

    // get `min` & `max` values
    MinMax minmax;

    minmax.min = &_twoHourBuffer->get(0)->temperature;
    minmax.max = &_twoHourBuffer->get(0)->temperature;

    for(uint16_t i=1; i<_twoHourBuffer->size(); i++){

        if(_twoHourBuffer->get(i)->temperature < *minmax.min)
            minmax.min = &_twoHourBuffer->get(i)->temperature;

        if(_twoHourBuffer->get(i)->temperature > *minmax.max)
            minmax.max = &_twoHourBuffer->get(i)->temperature;
    }

    // compute `up` and `down` ranges for y-axis
    _range.down = floor(*minmax.min);
    _range.up = ceil(*minmax.max);
    _range.size = _range.up - _range.down;

    // distance between x-ticks
    float xPosDistance = (
        (X_AXIS_WIDTH - X_AXIS_X) / (_twoHourBuffer->size() + 1)
    );

    // number of y-ticks
    uint16_t yNumberOfTicks = round(_range.size+1);

    // distance between y-ticks
    uint16_t yPosDistance = round(GRAPH_HEIGHT / (yNumberOfTicks - 1));

    // initialize eInk display
    _display->setPartialWindow(GRAPH_X, GRAPH_Y - 10, GRAPH_WIDTH,
                               GRAPH_HEIGHT + 10);
    _display->setFont(&TomThumb);
    _display->setTextColor(GxEPD_BLACK);
    _display->firstPage();

    do {

        // x-axis
        _writeLine(X_AXIS_X, X_AXIS_Y, X_AXIS_WIDTH, X_AXIS_Y);

        // y-axis
        _writeLine(Y_AXIS_X, Y_AXIS_Y, Y_AXIS_X, Y_AXIS_Y - Y_AXIS_HEIGHT);

        // x-ticks with bars, representing values
        for(uint16_t i=0; i<_twoHourBuffer->size(); i++){

            float xPosition = (
                X_AXIS_X + (i + 1) * xPosDistance
            );
            _writeLine(xPosition, X_AXIS_Y, xPosition, X_AXIS_Y + 3);

            // print value bar
            _drawBar(_twoHourBuffer->get(i)->temperature, xPosition);
        }

        // y-tics
        for(uint16_t i=0; i<=yNumberOfTicks; i++){
            uint16_t yPos = (X_AXIS_Y) - (i * yPosDistance);
            _writeLine(X_AXIS_X, yPos, X_AXIS_X - 3, yPos);
            _display->setCursor(X_AXIS_X - 20, yPos);
            _display->print(_range.down + i);
        }

    }
    while (_display->nextPage());

    _display->powerOff();
}


void EpdDht22::_printData(Dht22Data *data){

    _display->setFullWindow();
    _display->setRotation(0);
    _display->setFont(&Georgia_weather18pt7b);
    _display->setTextColor(GxEPD_BLACK);
    _display->firstPage();
    do
    {
      _display->fillScreen(GxEPD_WHITE);
      _display->setCursor(MARGIN_LEFT, TEMPERATURES_TOP);
      _display->print(THERMOMETER_100);
      _display->setCursor(MARGIN_LEFT + 20, TEMPERATURES_TOP);
      _display->print(data->temperature);
      _display->print(" ");
      _display->print(DEGREE_SIGN);
      _display->println("C");
      _display->setCursor(MARGIN_LEFT, TEMPERATURES_TOP + LINE);
      _display->print(WATER_DROP);
      _display->setCursor(MARGIN_LEFT + 20, TEMPERATURES_TOP + LINE);
      _display->print(data->humidity);
      _display->print(" ");
      _display->print("%");
    }
    while (_display->nextPage());
}


void EpdDht22::_printVcc(){
    long vcc = 0;
    for(uint16_t i=0; i<5; i++){
        vcc += readVcc();
        delay(100);
    };
    vcc = vcc / 5;
    _display->setPartialWindow(0, 0, 50, 20);
    _display->firstPage();
    do{
      _display->setFont(&Georgia_weather18pt7b);
      _display->setTextColor(GxEPD_BLACK);
      _display->setCursor(3, 13);
      _display->print(BATTERY_100);
      _display->print(F(" "));
      _display->setCursor(30, 11);
      _display->setTextColor(GxEPD_BLACK);
      _display->setFont(&TomThumb);
      _display->print(vcc / 1000.);
      _display->print(F(" V"));
    }
    while (_display->nextPage());
}


void EpdDht22::printScreen(){
#ifdef DBG
    _debugDataBuffer();
#endif

    // print data
    _printData(_twentyMinuteBuffer->last());

    // printVcc
    _printVcc();

    // print history graph
    _printHistory();
}
