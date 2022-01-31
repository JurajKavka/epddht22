#include "EpdDht22.h"
#include "Fonts/TomThumb.h"
#include "fonts/Georgia-weather18pt7b.h"
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

EpdDht22::EpdDht22(Settings *settings) {
  _settings = settings;

  // initialize devices
  _dht22 = new DHT(_settings->pinDht22, DHT_TYPE);
  // _display = new GxEPD2_AVR_3C(GxEPD2::GDEW0154Z04, /*CS=*/ 10, /*DC=*/ 8,
  //                              /*RST=*/ 9, /*BUSY=*/ 7);
  _display = new GxEPD2_AVR_BW(GxEPD2::GDEP015OC1, /*CS=*/SS, /*DC=*/8,
                               /*RST=*/9, /*BUSY=*/7);
  // initialize buffers
  _fiveMinuteBuffer = new CircularArray<Dht22Data>(_fmb, FIVE_MIN_BUFFER_SIZE);
  _twentyMinuteBuffer =
      new CircularArray<Dht22Data>(_tmb, TWENTY_MIN_BUFFER_SIZE);
  _twoHourBuffer = new CircularArray<Dht22Data>(_2hb, TWO_HOURS_BUFFER_SIZE);

  // set all the pins low for better power saving
  _setPinsLow();

  // start up DHT22 sensor
  _dht22->begin();
}

long EpdDht22::readVcc() {
  long result; // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);            // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA, ADSC))
    ;
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

Dht22Data EpdDht22::_getAverageValues(CircularArray<Dht22Data> *buffer) {
  Dht22Data sum = {0., 0.};
  Dht22Data average = {0., 0.};

  for (uint16_t i = 0; i < buffer->size(); i++) {
    Dht22Data *_tmp = buffer->get(i);
    sum.temperature += _tmp->temperature;
    sum.humidity += _tmp->humidity;
  }

  average.temperature = sum.temperature / (buffer->size());
  average.humidity = sum.humidity / (buffer->size());
  return average;
}

void EpdDht22::_setPinsLow() {
  for (byte i = 0; i < 20; i++) {
    pinMode(i, INPUT_PULLUP);
    digitalWrite(i, LOW);
  }
}

void EpdDht22::_debugDataBuffer() {
  // for(uint16_t i=0; i<_fiveMinuteBuffer->size(); i++){
  //     Serial.print(_fiveMinuteBuffer->get(i)->temperature);
  //     if(i != _fiveMinuteBuffer->size() - 1) Serial.print(", ");
  // }
  // Serial.println();
}

void EpdDht22::_debugHistoryBuffer() {
  // for(uint16_t i=0; i<_twoHourBuffer->size(); i++){
  //     Serial.print(_twoHourBuffer->get(i)->temperature);
  //     if(i != _twoHourBuffer->size() - 1) Serial.print(", ");
  // }
  // Serial.println();
}

void EpdDht22::_writeLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  _display->writeLine(x0, y0, x1, y1, GxEPD_BLACK);
}

void EpdDht22::_drawBar(float value, uint16_t xPos) {
  // compute height of bar
  uint16_t height =
      (round(((value - _range.down) / _range.size) * GRAPH_HEIGHT));
  // print bar
  _display->drawRect((xPos - 5), (Y_AXIS_Y - height), 10, height, GxEPD_BLACK);
}

void EpdDht22::powerUp() {
  pinMode(_settings->pinTransistorSwitch, OUTPUT);
  digitalWrite(_settings->pinTransistorSwitch, HIGH);
  delay(SWITCH_POWER_DELAY);
  _display->init();
}

void EpdDht22::powerDown() {
  delay(SWITCH_POWER_DELAY);
  digitalWrite(_settings->pinTransistorSwitch, LOW);
  pinMode(_settings->pinTransistorSwitch, INPUT);
  SPI.end();
  _setPinsLow();
}

Dht22Data EpdDht22::readDht22() {

  // read DHT22 to void to clean buffer
  _dht22->readTemperature();
  _dht22->readHumidity();

  delay(READ_DHT22_PAUSE);

  Dht22Data _tmp = {_dht22->readTemperature(), _dht22->readHumidity()};

  _fiveMinuteBuffer->push(_tmp);

  return _tmp;
}

Dht22Data EpdDht22::twentyMinuteAverage() {
  Dht22Data _avg20min = _getAverageValues(_fiveMinuteBuffer);
  _twentyMinuteBuffer->push(_avg20min);
  return _avg20min;
}

Dht22Data EpdDht22::twoHourAverage() {
  Dht22Data _avg2h = _getAverageValues(_twentyMinuteBuffer);
  _twoHourBuffer->push(_avg2h);
  return _avg2h;
}

void EpdDht22::_printData(Dht22Data *data) {

  // Vcc data
  long vcc = 0;
  for (uint16_t i = 0; i < 5; i++) {
    vcc += readVcc();
    delay(100);
  };
  vcc = vcc / 5;

#ifdef DBG
  _debugHistoryBuffer();
#endif

  // get `min` & `max` values
  MinMax minmax;

  minmax.min = _twoHourBuffer->get(0)->temperature;
  minmax.max = _twoHourBuffer->get(0)->temperature;

  for (uint16_t i = 1; i < _twoHourBuffer->size(); i++) {

    if (_twoHourBuffer->get(i)->temperature < minmax.min)
      minmax.min = _twoHourBuffer->get(i)->temperature;

    if (_twoHourBuffer->get(i)->temperature > minmax.max)
      minmax.max = _twoHourBuffer->get(i)->temperature;
  }

  // compute `up` and `down` ranges for y-axis
  int16_t minInt =
      (int16_t)floor(minmax.min); // najblizsie cele cislo k minimalnej
                                  // hodnote grafu (zdola)
  int16_t maxInt =
      (int16_t)ceil(minmax.max); // najblizsie cele cislo k minimalnej
                                 // hodnote grafu (zhora)
  int16_t heightStandard; // hodnota, vzhladom na ktoru sa bude pocitat vyska
                          // baru v grafe
  int16_t yNumberOfTicks; // pocet ticks na y-osi
  int16_t xAxeY;          // y-ova suradnica X-ovej osi grafu
  int16_t xTicksH = 3;    // vyska tick na x-ovej osi. V pripade, ze su vsetky
                          // hodnoty grafu zaporne, bude zaporna aj tato
                          // hodnota a ticks sa na x-ovej osi vykreslia nad os
                          // a nie pod nu ako v inych pripadoch.

  // Pri vykresleni grafu mozu nastat tri pripady. Bud su v grafe kladne
  // aj zaporne hodnoty, alebo iba kladne, resp iba zaporne hodnoty. Od toho
  // zavisi vypocet hodnot jednotlivych premennych.
  if (maxInt > 0 && minInt < 0) {

    heightStandard = maxInt + abs(minInt);

    xAxeY = (int16_t)round(
        X_AXIS_Y -
        (((float)abs(minInt) / (float)heightStandard) * (float)GRAPH_HEIGHT));
    yNumberOfTicks = heightStandard + 1;
  } else if (minInt < 0) {
    xTicksH = -3;
    xAxeY = Y_AXIS_Y - GRAPH_HEIGHT;
    heightStandard = abs(minInt) - abs(maxInt);
    yNumberOfTicks = abs(minInt - maxInt) + 1;
  } else {
    xAxeY = X_AXIS_Y;
    heightStandard = maxInt - minInt;
    yNumberOfTicks = maxInt - minInt + 1;
  }

  // distance between x-ticks
  float xPosDistance =
      ((float)(X_AXIS_WIDTH - X_AXIS_X) / (float)(_twoHourBuffer->size() + 1));

  _display->setFullWindow();
  _display->setRotation(0);
  _display->firstPage();

  do {
    _display->setFont(&Georgia_weather18pt7b);
    _display->setTextColor(GxEPD_BLACK);
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

    _printVcc(vcc);

    _display->setFont(&TomThumb);

    // x-axis
    _writeLine(X_AXIS_X, xAxeY, X_AXIS_WIDTH, xAxeY);

    // y-axis
    _writeLine(Y_AXIS_X, Y_AXIS_Y, Y_AXIS_X, Y_AXIS_Y - Y_AXIS_HEIGHT);

    // x-ticks with bars, representing values
    for (uint16_t i = 0; i < _twoHourBuffer->size(); i++) {

      float xPosition = (X_AXIS_X + (i + 1) * xPosDistance);
      // x-tick
      _writeLine(xPosition, xAxeY, xPosition, xAxeY + xTicksH);

      float scaledVal;
      if (maxInt > 0 && minInt < 0) {
        scaledVal = _twoHourBuffer->get(i)->temperature < 0
                        ? -1 * _twoHourBuffer->get(i)->temperature
                        : _twoHourBuffer->get(i)->temperature;
      } else if (minInt < 0) {
        scaledVal = abs(_twoHourBuffer->get(i)->temperature) - abs(maxInt);
      } else {
        scaledVal = _twoHourBuffer->get(i)->temperature - minInt;
      }

      float height = (scaledVal / heightStandard) * GRAPH_HEIGHT;

      if (_twoHourBuffer->get(i)->temperature > 0) {
        _display->drawRect((xPosition - 5), (xAxeY - height + 1), 10, height,
                           GxEPD_BLACK);
      } else {
        _display->drawRect((xPosition - 5), xAxeY + 1, 10, height, GxEPD_BLACK);
      }
    }

    // y-tics
    float yPositionsDistance =
        (float)(GRAPH_HEIGHT) / (float)(yNumberOfTicks - 1);

    for (int8_t i = 0; i < yNumberOfTicks; i++) {
      uint16_t yPos = (uint16_t)round((float)(X_AXIS_Y) -
                                      ((float)i * (float)yPositionsDistance));
      _writeLine(X_AXIS_X, yPos, X_AXIS_X - 3, yPos);
      _display->setCursor(X_AXIS_X - 20, yPos);
      _display->print(minInt + i);
    }

  } while (_display->nextPage());
  _display->powerOff();
}

void EpdDht22::_printVcc(long vcc) {
  /*
  long vcc = 0;
  for(uint16_t i=0; i<5; i++){
      vcc += readVcc();
      delay(100);
  };
  vcc = vcc / 5;
  _display->setPartialWindow(0, 0, 50, 20);
  _display->firstPage();
  do{
  */
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
  /*
  }
  while (_display->nextPage());
  */
}

void EpdDht22::printScreen() {
#ifdef DBG
  _debugDataBuffer();
#endif

  // print data
  _printData(_twentyMinuteBuffer->last());

  // printVcc
  //_printVcc();

  // print history graph
  //_printHistory();
}
