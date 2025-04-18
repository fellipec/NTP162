/*
 * NTP Clock with Weather Display for ESP8266
 * 
 * This project uses an ESP8266 microcontroller (Wemos D1) to display the current time
 * obtained from an NTP (Network Time Protocol) server on an LCD. It also fetches the
 * current weather information for a specified location (Curitiba) from wttr.in and displays
 * it on the second line of the LCD. The program includes WiFi connectivity, NTP synchronization,
 * and weather data retrieval via HTTPS.
 * 
 * Features:
 *  - Connects to a WiFi network from a predefined list of SSIDs.
 *  - Synchronizes the time with an NTP server.
 *  - Displays the current time (hour, minute, second) on the LCD.
 *  - Displays the current date and day of the week.
 *  - Fetches and displays the current weather and forecast from OpenWeatherMap API.
 *  - Supports basic button inputs for navigating between different displays (Network, NTP, Date, Weather).
 * 
 * Hardware:
 *  - ESP8266 (Wemos D1) microcontroller.
 *  - Keypad LCD 16x2 shield 
 * 
 * Libraries used:
 *  - ESP8266WiFi.h
 *  - ESP8266HTTPClient.h
 *  - NTPClient.h
 *  - WiFiUdp.h
 *  - WiFiClientSecure.h
 *  - LiquidCrystal.h
 *  - ArduinoJson.h
 *  
 * 
 * The program is designed for a minimalistic setup, displaying key information while allowing
 * users to interact with the device via a button.
 * 
 * Author: Luiz Fellipe Carneiro
 * Date: 2025-03-25
 * 
 * Note: The messages in this file are written in Brazilian Portuguese
 *       feel free to translate them to your preferred language.
 * 
 */

#include <ESP8266WiFi.h>              // Library for WiFi support on ESP8266
#include <ESP8266HTTPClient.h>        // Library for HTTP requests
#include <NTPClient.h>                // Library for NTP client functionality
#include <WiFiUdp.h>                  // Library for UDP communication (used by NTPClient)
#include <WiFiClientSecure.h>         // Library for secure HTTP (HTTPS) requests
#include <LiquidCrystal.h>            // Library for controlling the LCD
#include <ArduinoJson.h>              // Library for parsing JSON data

#include <wifi_credentials.h>         // Custom header for storing WiFi credentials
#include <apikeys.h>                  // Custom header for storing API keys

#define SERIALPRINT // Uncomment to enable serial print debugging


// The correct sequence of pins Wemos D1 similar to Arduino UNO
// link D-pins to GPIO for version R1
// Check https://github.com/kolandor/LCD-Keypad-Shield-Wemos-D1-Arduino-UNO
#define D0 3
#define RX D0
#define D1 1
#define TX D1
#define D2 16
#define D3 5
#define D4 4
#define D5 14
#define D6 12
#define D7 13
#define D8 0
#define D9 2
#define BOARD_LED D9
#define D10 15

//link A-pin
#define A0 0
#define BUTTON A0

// Initialize the LCD screen with specified pin configuration
LiquidCrystal lcd(D8, D9, D4, D5, D6, D7);
#include <digits.h> // Custom header for displaying big digits on the LCD

// NTP Server List. Change to your preferred servers
const char* ntpServers[] = {
    "scarlett",                         // Local NTP Server
    "a.ntp.br","b.ntp.br","c.ntp.br",   // Official Brazilian NTP Server
    "time.nist.gov",                    // USA NTP Server
    "pool.ntp.org"                      // NTP Pool
};

// Network variables
int numRedes = sizeof(ssids) / sizeof(ssids[0]);  // Number of Wi-Fi networks in wifi_credentials.h
int numNTPServers = sizeof(ntpServers) / sizeof(ntpServers[0]); // Number of NTP Servers
int ntpSrvIndex = 0; // Currently used NTP server

// Keys and LCD Variables
int buttonState = 0;
const char* gizmo[] = {"|", ">", "=", "<"}; //Wi-Fi loading animation
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
int counter = 0, lastCounter = 0, counterUD = 0, lastCounterUD = 0;
int maxUI = 3; // Number of screens
int minUI = -2; // Number of screens
unsigned long lastMillis = 0, lastUIMillis = 0; // Last time the screen was updated
unsigned int scrollPos = 0; // Position of the scrolling text
char scrollBuffer[17]; // Buffer for scrolling text
unsigned int updateInterval = 1000; // Update interval for the LCD

// OpenWeatherMap API
const char* apiKey = OWM_APIKEY; // Change for your API key
const char* lon = "-49.2908"; // Change coordinates for your city
const char* lat = "-25.504";
const int alt = 935; // Altitude in meters
#define MAX_REQUEST_SIZE 512
#define MAX_RESPONSE_SIZE 4096
#define FETCH_INTERVAL 900 // Fetch weather data every 15 minutes
char weatherJson[MAX_RESPONSE_SIZE];

// Weather variables
float tmp, hum, pres, calc_alt, qnh;
float lastTemp = -1000, lastHum = -1000;
float current_temp = 0.0;
float current_feels_like = 0.0;
float current_temp_min = 0.0;
float current_temp_max = 0.0;
int current_pressure = 0.0;
int current_humidity = 0.0;
char current_weatherDescription[21]; // 20 chars + '\0'
char location_name[21]; // 20 chars + '\0'
long current_sunset = 0;
long current_sunrise = 0;
long current_dt = 0;
#define FORECAST_HOURS 8
long forecast_dt = 0;
struct Forecast {
  long dt;
  float temp;
  float feels_like;
  float temp_min;
  float temp_max;
  int pressure;
  int humidity;
  float pop;
  float rain_3h;
  char description[32];
};

Forecast forecast[FORECAST_HOURS];

// Time Zone (UTC-3)
const long utcOffsetInSeconds = -10800;


// Network initialization
WiFiUDP ntpUDP;
WiFiClientSecure client;
NTPClient timeClient(ntpUDP, ntpServers[0], utcOffsetInSeconds); // UTC-3 (Brasil)

/*
 * tryNTPServer() - Tries to connect to a list of NTP servers
 * 
 * This function attempts to establish a connection with a list of NTP (Network Time Protocol)
 * servers to synchronize the time. It loops through the predefined list of NTP server addresses
 * and tries to update the time using each server. If a successful connection is made, it returns
 * the index of the server that was successfully connected to. If all servers fail, it returns -1.
 */
int tryNTPServer() {
    for (int i = 0; i < numNTPServers; i++) {
        timeClient.setPoolServerName(ntpServers[i]);
        timeClient.begin();
        if (timeClient.update()) {
            #ifdef SERIALPRINT
            Serial.println("Conexão com NTP bem-sucedida: " + String(ntpServers[i]));
            #endif
            return i;
        } else {
            #ifdef SERIALPRINT
            Serial.println("Erro ao conectar no NTP: " + String(ntpServers[i]));
            #endif
        }
    }
    return -1;
}

void removeAccents(char* str) {
    char* src = str;
    char* dst = str;
  
    while (*src) {
      // Se encontrar caractere UTF-8 multibyte (início com 0xC3)
      if ((uint8_t)*src == 0xC3) {
        src++;  // Avança para o próximo byte
        switch ((uint8_t)*src) {
          case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4:  *dst = 'a'; break; // àáâãä
          case 0x80: case 0x81: case 0x82: case 0x83: case 0x84:  *dst = 'A'; break; // ÀÁÂÃÄ
          case 0xA7: *dst = 'c'; break; // ç
          case 0x87: *dst = 'C'; break; // Ç
          case 0xA8: case 0xA9: case 0xAA: case 0xAB: *dst = 'e'; break; // èéêë
          case 0x88: case 0x89: case 0x8A: case 0x8B: *dst = 'E'; break; // ÈÉÊË
          case 0xAC: case 0xAD: case 0xAE: case 0xAF: *dst = 'i'; break; // ìíîï
          case 0x8C: case 0x8D: case 0x8E: case 0x8F: *dst = 'I'; break; // ÌÍÎÏ
          case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: *dst = 'o'; break; // òóôõö
          case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: *dst = 'O'; break; // ÒÓÔÕÖ
          case 0xB9: case 0xBA: case 0xBB: case 0xBC: *dst = 'u'; break; // ùúûü
          case 0x99: case 0x9A: case 0x9B: case 0x9C: *dst = 'U'; break; // ÙÚÛÜ
          case 0xB1: *dst = 'n'; break; // ñ
          case 0x91: *dst = 'N'; break; // Ñ
          default: *dst = '?'; break; // desconhecido
        }
        dst++;
        src++; // Pula o segundo byte do caractere especial
      } else {
        *dst++ = *src++; // Copia byte normal
      }
    }
    *dst = '\0'; // Termina a nova string
  }
  

void getScrollWindow(const char* src, char* dest, int pos, int width = 17) {
    int len = strlen(src);
    if (len == 0){
        dest[0] = '\0';  // Returns an empty string for empty source
        return;
    }
    
    pos = pos % len; // Wraps around the scroll position

    for (int i = 0; i < width; i++) {
      int idx = (pos + i) % (len);  // len + width for sliding effect
      if (idx < len) {
        dest[i] = src[idx];
      } else {
        dest[i] = ' ';  // Adds spaces for the remaining width
      }
    }
    dest[width] = '\0';
  }

void upperFirstLetter(char* str) {
    if (str && str[0] != '\0') {  // Checks for empty string        
      str[0] = toupper(str[0]);  // Convert the first character to uppercase
    }
  }


void buildWeatherRequest(char* request, const char* lat, const char* lon, const char* apiKey) {
    snprintf(request, MAX_REQUEST_SIZE, 
             "GET /data/2.5/weather?lat=%s&lon=%s&appid=%s&units=metric&lang=pt_br HTTP/1.1\r\n"
             "Host: api.openweathermap.org\r\n"
             "Connection: close\r\n\r\n", 
             lat, lon, apiKey);
}

void buildForecastRequest(char* request, const char* lat, const char* lon, const char* apiKey) {
    snprintf(request, MAX_REQUEST_SIZE, 
             "GET /data/2.5/forecast?lat=%s&lon=%s&cnt=8&appid=%s&units=metric&lang=pt_br HTTP/1.1\r\n"
             "Host: api.openweathermap.org\r\n"
             "Connection: close\r\n\r\n", 
             lat, lon, apiKey);
}

void getWeatherJSON(bool forecast = false) {
    if (!client.connect("api.openweathermap.org", 443)) { 
        #ifdef SERIALPRINT
        Serial.println("Falha ao conectar ao servidor.");
        #endif
        return;
    }
    char req[MAX_REQUEST_SIZE];
    if (forecast) {
        buildForecastRequest(req, lat, lon, apiKey);
    } else {
        buildWeatherRequest(req, lat, lon, apiKey);
    }    
    
    #ifdef SERIALPRINT
    Serial.println("Requisição:");
    Serial.println(req);
    #endif
    client.print(req); 

    unsigned long timeout = millis();
    while (client.available() == 0) { 
        if (millis() - timeout > 5000) { // 5 seconds timeout
            #ifdef SERIALPRINT
            Serial.println("Erro: Timeout.");
            #endif
            client.stop();
            return;
        }
    }
    // Payload buffer
    // User this while loop to avoid the Strings object
    // to avoid memory fragmentation
    unsigned int index = 0;
    unsigned long lastRead = millis();
    while (millis() - lastRead < 2000) { // timeout de 2 segundos
        while (client.available()) {            
            if (index < MAX_RESPONSE_SIZE - 1) {  // Buffer limit check
                weatherJson[index++] = (char)client.read();  // Add the next character to the buffer
                lastRead = millis();  // Update last read time
            } else {
                break;  // Buffer is full, stop reading
            }
        }
        yield(); // tenta cooperar com o Wi-Fi
    }
    weatherJson[index] = '\0';  // Add null terminator to the string
    #ifdef SERIALPRINT
    Serial.println("Resposta do servidor:");
    Serial.print(weatherJson);
    Serial.print("\n\n");
    #endif

    // Find the JSON start position
    char* jsonStart = strchr(weatherJson, '{');  // First { character
    if (jsonStart) {
        // Copy the JSON part to the payload
        strcpy(weatherJson, jsonStart);
    } else {
        #ifdef SERIALPRINT
        Serial.println("Erro: JSON não encontrado na resposta.");
        #endif
        return;
    }

}


void getForecast() {
    if ((timeClient.getEpochTime() - forecast_dt > FETCH_INTERVAL*4)) {
        forecast_dt = timeClient.getEpochTime();
        getWeatherJSON(true);
        
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, weatherJson, MAX_RESPONSE_SIZE);
        
        if (error) {
            #ifdef SERIALPRINT
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            #endif
            return;
        }
        
        JsonArray list = doc["list"];
        
        for (int i = 0; i < FORECAST_HOURS; i++) {
          JsonObject entry = list[i];
          JsonObject main = entry["main"];
          JsonObject weather0 = entry["weather"][0];
          JsonObject rain = entry["rain"];
          
          forecast[i].dt = entry["dt"];
          forecast[i].dt += utcOffsetInSeconds;
          forecast[i].temp = main["temp"];
          forecast[i].feels_like = main["feels_like"];
          forecast[i].temp_min = main["temp_min"];
          forecast[i].temp_max = main["temp_max"];
          forecast[i].pressure = main["pressure"];
          forecast[i].humidity = main["humidity"];
          forecast[i].pop = entry["pop"];
          forecast[i].rain_3h = rain["3h"] | 0.0;
        
          const char* desc = weather0["description"] | "";
          strncpy(forecast[i].description, desc, sizeof(forecast[i].description));
          forecast[i].description[sizeof(forecast[i].description) - 1] = '\0';
          upperFirstLetter(forecast[i].description);
        }
        

    }

}


void getWeather() {
    if (timeClient.getEpochTime() - current_dt > FETCH_INTERVAL) {


        getWeatherJSON(false);
    
        // JSON parsing   
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, weatherJson, MAX_RESPONSE_SIZE);

        if (error) {
            #ifdef SERIALPRINT
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            #endif
            return;
        }
        
        #ifdef SERIALPRINT
        Serial.println("JSON parsed");
        #endif
        JsonObject weather_0 = doc["weather"][0];
        const char* desc = weather_0["description"] | ""; 
        strncpy(current_weatherDescription, desc, sizeof(current_weatherDescription)); // Copy string to avoid null pointer
        current_weatherDescription[sizeof(current_weatherDescription) - 1] = '\0'; // add null terminator
        upperFirstLetter(current_weatherDescription); // Capitalize first letter
        const char* name = doc["name"] | "";
        strncpy(location_name, name, sizeof(location_name)); // Copy string to avoid null pointer
        location_name[sizeof(location_name) - 1] = '\0'; // add null terminator
        upperFirstLetter(location_name); // Capitalize first letter

        JsonObject main = doc["main"];
        current_temp= main["temp"]; 
        current_feels_like = main["feels_like"]; 
        current_temp_min = main["temp_min"]; 
        current_temp_max = main["temp_max"]; 
        current_pressure = main["pressure"]; 
        current_humidity = main["humidity"]; 
        current_dt = doc["dt"];
        current_dt += utcOffsetInSeconds;

        JsonObject sys = doc["sys"];
        current_sunset = sys["sunset"];
        current_sunrise = sys["sunrise"];

        
        #ifdef SERIALPRINT
        Serial.printf("Clima: %s\n", current_weatherDescription);
        Serial.printf("Temp: %.1f C\n", current_temp);
        Serial.printf("Min: %.1f C\n", current_temp_min);
        Serial.printf("Max: %.1f C\n", current_temp_max);
        Serial.printf("Sensação: %.1f C\n", current_feels_like);
        Serial.printf("Umidade: %d%%\n", current_humidity);
        Serial.printf("Pressão: %d hPa\n", current_pressure);
        Serial.printf("Localização: %s\n", location_name);
        Serial.printf("Data: %ld\n", current_dt);
        Serial.printf("Nascer do sol: %ld\n", current_sunrise);
        Serial.printf("Pôr do sol: %ld\n", current_sunset);
        Serial.printf("Latitude: %s\n", lat);
        Serial.printf("Longitude: %s\n", lon);
        #endif
        

    }
 
  }


/*
 * setup() - Initializes the system and connects to Wi-Fi and NTP server
 * 
 * It initializes the serial interface, the LCD display, and the Wi-Fi connection.
 * Then, it attempts to sync with an NTP server. If any step fails, 
 * the microcontroller is restarted.
 */
void setup() {
    Serial.begin(115200);  // Initialize serial communication at 115200 baud rate
    lcd.begin(16, 2);  // Initialize the LCD with 16 columns and 2 rows
    lcd.clear();
    lcd.print("Conectando em:");
    
    bool conectado = false;  // Flag to track if Wi-Fi connection is successful

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Limpa conexões anteriores
    delay(100);
    #ifdef SERIALPRINT
    Serial.println("Escaneando redes...");
    #endif
    int n = WiFi.scanNetworks();
    if (n == 0) {
      #ifdef SERIALPRINT
      Serial.println("Nenhuma rede encontrada.");
      #endif
      return;
    }

    // Loop to attempt connection to each SSID in the list
    for (int i = 0; i < numRedes; i++) {
        #ifdef SERIALPRINT
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        #endif
        lcd.setCursor(0, 1);
        lcd.print("               ");
        lcd.setCursor(0, 1);
        lcd.print(ssids[i]);
        bool found = false;
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == ssids[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            #ifdef SERIALPRINT
            Serial.println(" - Rede não encontrada.");
            #endif
            continue;  // Skip to the next SSID if not found
        }
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        int j = 0;  // Index for the gizmo array
        // Retry connection up to 10 seconds (100 attempts)
        while (WiFi.status() != WL_CONNECTED && tentativa < 100) {
            delay(100);
            #ifdef SERIALPRINT
            Serial.print(".");
            #endif
            lcd.setCursor(15, 1);
            lcd.print(gizmo[j]);  // Display some progress information
            j = (j + 1) % 4;  // Cycle through the gizmo array
            tentativa++;
        }
        
        // If connected successfully to Wi-Fi
        if (WiFi.status() == WL_CONNECTED) {
            #ifdef SERIALPRINT
            Serial.println("\nConectado!");
            #endif
            lcd.clear();
            lcd.print("Conectado ao ");
            lcd.setCursor(0, 1);
            lcd.print("Wi-Fi: ");
            lcd.print(ssids[i]);
            conectado = true;
            break;  // Exit loop if connection is successful
        } else {
            #ifdef SERIALPRINT
            Serial.println("\nFalha ao conectar.");
            #endif
        }
    }

    // If no Wi-Fi connection was made, restart the system
    if (!conectado) {
        lcd.clear();
        lcd.print("Erro ao conectar");
        delay(10000);
        ESP.restart();  // Restart the ESP to retry
    }
    
    // Try connecting to an NTP server if Wi-Fi connection is successful
    lcd.clear();
    ntpSrvIndex = tryNTPServer();  // Call to tryNTPServer() function
    
    // If connected to NTP server, display success
    if (ntpSrvIndex >= 0) {
        lcd.print("Conectado ao NTP");
        lcd.setCursor(0, 1);
        lcd.print(ntpServers[ntpSrvIndex]);
        delay(2000);
    } else {
        lcd.print("Erro ao conectar NTP");
        delay(10000);
        ESP.restart();  // Restart the ESP if NTP connection fails
    }
    
    // Create custom LCD characters
    lcd.createChar(0, LT);
    lcd.createChar(1, UB);
    lcd.createChar(2, RT);
    lcd.createChar(3, LL);
    lcd.createChar(4, LB);
    lcd.createChar(5, LR);
    lcd.createChar(6, MB);
    lcd.createChar(7, block);
    
    lcd.backlight();  // Turn on the LCD backlight
    
    // Display digits on the LCD
    lcd.clear();
    printDigits(0, 0);
    printDigits(0, 4);
    printDigits(0, 8);
    printDigits(0, 12);
    delay(1000);
    
    // Set SSL client to insecure mode (bypass certificate verification)
    client.setInsecure();

    getForecast();  // Fetch weather forecast data
    getWeather();  // Fetch current weather data
}


/*
 * printNumber() - Displays a two-digit number on the LCD
 * 
 * It splits the number into tens and ones, then calls printDigits() to display 
 * each digit at the correct position on the LCD.
 */
void printNumber(int val){
     int col=9;    
     printDigits(val/10,col);
     printDigits(val%10,col+4);
}


/*
 * printTime() - Displays the current time on the LCD
 * 
 * Prints hours and minutes using printDigits(), 
 * placing colons at fixed positions to format the time.
 */

void printTime(int h, int m, int s) {
    counterUD = 0;
    updateInterval = 1000;
    scrollBuffer [0] = '\0'; // Clear the scroll buffer
    scrollPos = 0; // Reset the scroll position
    char separator = (s % 2 == 0) ? ':' : ' ';
    printDigits(h / 10, 0);
    printDigits(h % 10, 4);
    lcd.setCursor(7, 0);
    lcd.print(separator);
    lcd.setCursor(7, 1);
    lcd.print(separator);
    printDigits(m / 10, 8);
    printDigits(m % 10, 12);
}


/*
 * printDate() - Retrieves and displays the current date and time on the LCD
 * 
 * It fetches the epoch time from the NTP client and calculates the date manually. 
 * The function then formats and prints the time, weekday, and date on the LCD.
 */
unsigned long lastDateMillis = 0;
void printDate() {
    if (millis() - lastDateMillis > 500) {
        lastDateMillis = millis();
    
        timeClient.update();
        
        unsigned long epoch = timeClient.getEpochTime();
        
        // Calculates the time
        int seconds = epoch % 60;
        int minutes = (epoch / 60) % 60;
        int hours = (epoch / 3600) % 24;
        int days = epoch / 86400;
        
        // Set the date to the UNIX epoch: 1970-01-01
        int year = 1970;
        int month = 1;
        int day = 1;

        // Calculate the number of years elapsed
        while (days >= (365 + (year % 4 == 0 ? 1 : 0))) {
            days -= (365 + (year % 4 == 0 ? 1 : 0));
            year++;
        }

        // Calculate the month
        int daysInMonth[] = {31, (year % 4 == 0 ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        while (days >= daysInMonth[month - 1]) {
            days -= daysInMonth[month - 1];
            month++;
        }

        // Finally, the days
        day += days;

        // Show the results        
        lcd.setCursor(4, 0);
        lcd.printf("%02d:%02d:%02d ", hours, minutes, seconds);
        lcd.setCursor(1, 1);
        lcd.print(daysOfTheWeek[timeClient.getDay()]);
        lcd.print(" ");
        lcd.printf("%02d/%02d/%04d", day, month, year);        
    }
}


/*
 * printNetwork() - Displays the device's IP address and Wi-Fi SSID on the LCD
 * 
 * It clears the LCD, prints the local IP address on the first row, 
 * and the connected Wi-Fi SSID on the second row. The information 
 * is displayed for 5 seconds.
 */
unsigned long lastNetworkMillis = 0;
void printNetwork() {
    if (millis() - lastNetworkMillis > 10000) {
        lastNetworkMillis = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(WiFi.localIP());
        lcd.setCursor(0, 1);
        lcd.print(WiFi.SSID()); 
    }
}


/*
 * printNTP() - Displays the current NTP server and time on the LCD
 * 
 * It clears the LCD and prints the active NTP server on the first row. 
 * The second row continuously updates with the formatted time for 10 seconds.
 */
unsigned long lastNTPMillis = 0;
void printNTP() {
    if (millis() - lastNTPMillis > 1000) {
        lastNTPMillis = millis();
        lcd.setCursor(0, 0);
        lcd.print(ntpServers[ntpSrvIndex]);
        lcd.setCursor(0, 1);
        lcd.print(timeClient.getFormattedTime());
    }
}



/*
 * printWeather() - Fetches and displays weather information on the LCD
 * 
 * If connected to Wi-Fi, it sends an HTTP GET request to retrieve weather data. 
 * If successful, the response is displayed on the LCD for 15 seconds. 
 * Otherwise, an error is logged to the serial monitor.
 * 
 */
unsigned long lastWeatherMillis = 0;
void printWeather() {
    updateInterval = 500;
    if (millis() - lastWeatherMillis > updateInterval) {
        lastWeatherMillis = millis();
        char weather[100];
        snprintf(weather, 
            sizeof(weather), 
            "%s - Temp: %.1fC - Humid: %d%% - Press: %dhPa   ", 
            current_weatherDescription, 
            current_temp, 
            current_humidity, 
            current_pressure);
        #ifdef SERIALPRINT
        Serial.println(weather);
        #endif
        removeAccents(weather);
        getScrollWindow(weather, scrollBuffer, scrollPos);
        time_t epoch = (time_t)current_dt;
        struct tm timeinfo;
        gmtime_r(&epoch, &timeinfo);
        lcd.setCursor(0, 0);
        lcd.printf("Hoje as %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lcd.setCursor(0, 1);
        lcd.print(scrollBuffer);
        scrollPos++;
    }

}


void printForecast() {
    updateInterval = 500;
    if (millis() - lastWeatherMillis > updateInterval) {
        lastWeatherMillis = millis();
        char weather[100];
        snprintf(weather, sizeof(weather),
         "%s - Min: %.1fC Max: %.1fC - %.0f%% Chuva: %.1fmm  Humid: %d%% - Press: %dhPa   ",
         forecast[counterUD].description,
         forecast[counterUD].temp_min,
         forecast[counterUD].temp_max,
         forecast[counterUD].pop*100,
         forecast[counterUD].rain_3h,
         forecast[counterUD].humidity,
         forecast[counterUD].pressure);
        #ifdef SERIALPRINT
        Serial.println(weather);
        #endif
        removeAccents(weather);
        getScrollWindow(weather, scrollBuffer, scrollPos);
        time_t epoch = (time_t)forecast[counterUD].dt;
        struct tm timeinfo;
        gmtime_r(&epoch, &timeinfo);
        lcd.setCursor(0, 0);
        lcd.printf("%02d/%02d - %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1,timeinfo.tm_hour, timeinfo.tm_min);
        lcd.setCursor(0, 1);
        lcd.print(scrollBuffer);
        scrollPos++;
    }
}


/*
 * button() - Determines which button is pressed based on analog input
 * 
 * It reads an analog value and returns a corresponding button code:
 * 0 = No button, 1 = Select (unreliable), 2 = Left, 3 = Down, 4 = Up, 5 = Right.
 */

 int button(int analogValue) {
    if (analogValue > 1010) {
        return 0;
    } else if (analogValue > 900) {
        return 1; // Select - Not very reliable on this shield, avoid using
    } else if (analogValue > 600) {
        return 2; // Left
    } else if (analogValue > 300) {
        return 3; // Down
    } else if (analogValue > 100) {
        return 4; // Up
    } else if (analogValue >= 0) {
        return 5; // Right
    }
    return 0;
}

// Variables to hold status for the loop function
int lastHours = 0;
int lastMinutes = 0;
int countBlink = 0;




// *************
// The main loop
// *************
void loop()
{

    // Uncomment the following lines to get the value of the analog pin
    // into the serial monitor, to adjust the function button
    // int x = analogRead(BUTTON);
    // Serial.print(x);

    // Reads the buttons and take action if some is pressed
    if (millis() - lastUIMillis > 666) {
        int buttonState = button(analogRead(BUTTON));
        
        switch (buttonState) {
            case 1:
                #ifdef SERIALPRINT
                Serial.printf("Select %d\n", counter);
                #endif
                break;

            case 2:
                counter--;
                if (counter < minUI) {
                    counter = maxUI;
                }
                #ifdef SERIALPRINT
                Serial.printf("Left %d\n", counter);
                #endif
                break;

            case 3:
                counterUD--;
                #ifdef SERIALPRINT
                Serial.println("Down");
                #endif
                break;

            case 4:
                counterUD++;
                #ifdef SERIALPRINT
                Serial.println("Up");
                #endif
                break;

            case 5:
                counter++;
                if (counter > maxUI) {
                    counter = minUI;
                }
                #ifdef SERIALPRINT
                Serial.printf("Right %d\n", counter);
                #endif
                break;

            default:
                // No button is pressed
                break;
        }
    }
    
    if ((millis() - lastMillis > updateInterval) || (lastCounter != counter) || (lastCounterUD != counterUD) ) {
        lastMillis = millis();

        timeClient.update();
        if (!timeClient.isTimeSet()) {
            #ifdef SERIALPRINT
            Serial.println("Erro ao atualizar o tempo.");
            #endif
            int n = tryNTPServer();
            if (n < 0) {
                lcd.clear();
                lcd.print("Erro ao conectar NTP");
                delay(10000);
                ESP.restart();
            }
        }


        int hours = timeClient.getHours();
        int minutes = timeClient.getMinutes();
        int seconds = timeClient.getSeconds();

        if (lastCounter != counter) {
            lastCounter = counter;
            lastUIMillis = millis();
            lcd.clear();
        }
        if (lastCounterUD != counterUD) {
            lastCounterUD = counterUD;
            lastUIMillis = millis();
        }

        if (millis() - lastUIMillis > 60000) {
            counter = 0;
        }

        switch (counter)
        {
        case 0:            
            printTime(hours, minutes, seconds);
            break;

        case -2:
            printNTP();

        case -1:
            printNetwork();
            break;

        case 1: 
            printDate();
            break;
        
        case 2: 
            printWeather();
            break;
        
        case 3:
            printForecast();
            break;
        
        
        default:
            printTime(hours, minutes, seconds);
            break;
        }
 
    }

    getForecast();  // Fetch weather forecast data
    getWeather();  // Fetch current weather data

}
