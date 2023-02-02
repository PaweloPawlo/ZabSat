#include <sps30.h>
#include <SD.h>
#include <CanSatKit.h>

using namespace CanSatKit;

BMP280 bmp;

int counter = 1;
bool led_state = false;
const int led_pin = 13;

Radio radio(Pins::Radio::ChipSelect,
            Pins::Radio::DIO0,
            433.5,
            Bandwidth_125000_Hz,
            SpreadingFactor_9,
            CodingRate_4_8);

Frame frame;


// Example arduino sketch, based on 
// https://github.com/Sensirion/embedded-sps/blob/master/sps30-i2c/sps30_example_usage.c


// uncomment the next line to use the serial plotter
// #define PLOTTER_FORMAT

void setup() {
  int16_t ret;
  uint8_t auto_clean_days = 4;
  uint32_t auto_clean;

  SerialUSB.begin(9600);
  delay(2000);

  File dataFile = SD.open("datalog.txt", FILE_WRITE | O_TRUNC);

  if(!bmp.begin()) {
    // if connection failed - print message to the user
    SerialUSB.println("BMP init failed!");
    // the program will be 'halted' here and nothing will happen till restart
    while(1);
  } else {
    // print message to the user if everything is OK
    SerialUSB.println("BMP init success!");
  }
  bmp.setOversampling(16);

  sensirion_i2c_init();

  while (sps30_probe() != 0) {
  SerialUSB.print("SPS sensor probing failed\n");
    delay(500);
  }

#ifndef PLOTTER_FORMAT
SerialUSB.print("SPS sensor probing successful\n");
#endif /* PLOTTER_FORMAT */

  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  if (ret) {
  SerialUSB.print("error setting the auto-clean interval: ");
  SerialUSB.println(ret);
  }

  ret = sps30_start_measurement();
  if (ret < 0) {
  SerialUSB.print("error starting measurement\n");
  }

#ifndef PLOTTER_FORMAT
SerialUSB.print("measurements started\n");
#endif /* PLOTTER_FORMAT */

#ifdef SPS30_LIMITED_I2C_BUFFER_SIZE
SerialUSB.print("Your Arduino hardware has a limitation that only\n");
SerialUSB.print("  allows reading the mass concentrations. For more\n");
SerialUSB.print("  information, please check\n");
SerialUSB.print("  https://github.com/Sensirion/arduino-sps#esp8266-partial-legacy-support\n");
SerialUSB.print("\n");
  delay(2000);
#endif

  if (!SD.begin(11)) {
    SerialUSB.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  SerialUSB.println("card initialized.");

  pinMode(led_pin, OUTPUT);
  //Serial.begin(115200);
  radio.begin();
  radio.disable_debug();

  dataFile.close();
  delay(1000);
}

void loop() {
  struct sps30_measurement m;
  char serial[SPS30_MAX_SERIAL_LEN];
  uint16_t data_ready;
  int16_t ret;
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  String dataString = "";
  double T, P;

  do {
    ret = sps30_read_data_ready(&data_ready);
    if (ret < 0) {
    SerialUSB.print("error reading data-ready flag: ");
    SerialUSB.println(ret);
    } else if (!data_ready)
    SerialUSB.print("data not ready, no new measurement available\n");
    else
      break;
    delay(100); /* retry in 100ms */
  } while (1);

  ret = sps30_read_measurement(&m);
  if (ret < 0) {
  SerialUSB.print("error reading measurement\n");
  } else {

#ifndef PLOTTER_FORMAT
  SerialUSB.print("PM  1.0: ");
  SerialUSB.println(m.mc_1p0);
  SerialUSB.print("PM  2.5: ");
  SerialUSB.println(m.mc_2p5);
  SerialUSB.print("PM  4.0: ");
  SerialUSB.println(m.mc_4p0);
  SerialUSB.print("PM 10.0: ");
  SerialUSB.println(m.mc_10p0);
  dataString += "PM  2.5: " + String(m.mc_2p5) + "\nPM 10.0: " + String(m.mc_10p0);

  bmp.measureTemperatureAndPressure(T, P);
  SerialUSB.print("Pressure: ");
  SerialUSB.print(P, 2);  
  SerialUSB.print("\nTemperature: ");
  SerialUSB.print(T, 2);  

  dataString += "\nPressure: " + String(P, 2) + "\nTemperature: " + String(T, 2);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    SerialUSB.println("\nDane zapisane");
    dataFile.close();
    // print to the serial port too:
    // SerialUSB.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    SerialUSB.println("error opening datalog.txt");
  }

  radio.transmit(dataString);
  /*
    while(radio.available()){
      char data[256];
      radio.receive(data);
      SerialUSB.print("RX: ");
      SerialUSB.println(data);
      }
  */

// #ifndef SPS30_LIMITED_I2C_BUFFER_SIZE
  // SerialUSB.print("NC  0.5: ");
  // SerialUSB.println(m.nc_0p5);
  // SerialUSB.print("NC  1.0: ");
  // SerialUSB.println(m.nc_1p0);
  // SerialUSB.print("NC  2.5: ");
  // SerialUSB.println(m.nc_2p5);
  // SerialUSB.print("NC  4.0: ");
  // SerialUSB.println(m.nc_4p0);
  // SerialUSB.print("NC 10.0: ");
  // SerialUSB.println(m.nc_10p0);

  // SerialUSB.print("Typical partical size: ");
  // SerialUSB.println(m.typical_particle_size);
#endif

  SerialUSB.println();

// #else
    // since all values include particles smaller than X, if we want to create buckets we 
    // need to subtract the smaller particle count. 
    // This will create buckets (all values in micro meters):
    // - particles        <= 0,5
    // - particles > 0.5, <= 1
    // - particles > 1,   <= 2.5
    // - particles > 2.5, <= 4
    // - particles > 4,   <= 10

  // SerialUSB.print(m.nc_0p5);
  // SerialUSB.print(" ");
  // SerialUSB.print(m.nc_1p0  - m.nc_0p5);
  // SerialUSB.print(" ");
  // SerialUSB.print(m.nc_2p5  - m.nc_1p0);
  // SerialUSB.print(" ");
  // SerialUSB.print(m.nc_4p0  - m.nc_2p5);
  // SerialUSB.print(" ");
  // SerialUSB.print(m.nc_10p0 - m.nc_4p0);
  // SerialUSB.println();


// #endif /* PLOTTER_FORMAT */

  }

  delay(1000);
}
