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
 *  - Fetches and displays the current weather (temperature and condition) from wttr.in.
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
#include <wifi_credentials.h>         // Custom header for storing WiFi credentials


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
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};

// Weather URL
// Check https://github.com/chubin/wttr.in for instructions on how to
// format it
const char* weatherUrl = "https://wttr.in/Curitiba?M&lang=pt&format=%t+%C";

// Network initialization
WiFiUDP ntpUDP;
WiFiClientSecure client;
NTPClient timeClient(ntpUDP, ntpServers[0], -3 * 3600, 60000); // UTC-3 (Brasil)

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
            Serial.println("Conexão com NTP bem-sucedida: " + String(ntpServers[i]));
            return i;
        } else {
            Serial.println("Erro ao conectar no NTP: " + String(ntpServers[i]));
        }
    }
    return -1;
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
    
    // Loop to attempt connection to each SSID in the list
    for (int i = 0; i < numRedes; i++) {
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        lcd.setCursor(0, 1);
        lcd.print(ssids[i]);
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        // Retry connection up to 10 seconds (100 attempts)
        while (WiFi.status() != WL_CONNECTED && tentativa < 100) {
            delay(100);
            Serial.print(".");
            lcd.setCursor(15, 1);
            lcd.print(gizmo[i]);  // Display some progress information
            i = (i + 1) % 4;  // Cycle through the gizmo array
            tentativa++;
        }
        
        // If connected successfully to Wi-Fi
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConectado!");
            lcd.clear();
            lcd.print("Conectado ao ");
            lcd.setCursor(0, 1);
            lcd.print("Wi-Fi: ");
            lcd.print(ssids[i]);
            conectado = true;
            break;  // Exit loop if connection is successful
        } else {
            Serial.println("\nFalha ao conectar.");
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
 * It clears the display and prints hours and minutes using printDigits(), 
 * placing colons at fixed positions to format the time.
 */

void printTime(int h, int m) {
    lcd.clear();
    printDigits(h / 10, 0);
    printDigits(h % 10, 4);
    lcd.setCursor(7, 0);
    lcd.print(":");
    lcd.setCursor(7, 1);
    lcd.print(":");
    printDigits(m / 10, 8);
    printDigits(m % 10, 12);
}


/*
 * printDate() - Retrieves and displays the current date and time on the LCD
 * 
 * It fetches the epoch time from the NTP client and calculates the date manually. 
 * The function then formats and prints the time, weekday, and date on the LCD.
 */

void printDate() {
    timeClient.update();
    for (int i = 0; i < 10; i++){
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
        lcd.clear();
        lcd.setCursor(4, 0);
        lcd.printf("%02d:%02d:%02d ", hours, minutes, seconds);
        lcd.setCursor(1, 1);
        lcd.print(daysOfTheWeek[timeClient.getDay()]);
        lcd.print(" ");
        lcd.printf("%02d/%02d/%04d", day, month, year);
        delay(1000);
    }
}


/*
 * printNetwork() - Displays the device's IP address and Wi-Fi SSID on the LCD
 * 
 * It clears the LCD, prints the local IP address on the first row, 
 * and the connected Wi-Fi SSID on the second row. The information 
 * is displayed for 5 seconds.
 */

void printNetwork() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 1);
    lcd.print(WiFi.SSID());
    delay(5000);
}


/*
 * printNTP() - Displays the current NTP server and time on the LCD
 * 
 * It clears the LCD and prints the active NTP server on the first row. 
 * The second row continuously updates with the formatted time for 10 seconds.
 */

void printNTP() {
    lcd.clear();
    lcd.print(ntpServers[ntpSrvIndex]);
    for (int i = 0; i < 100; i++){
        lcd.setCursor(0, 1);
        lcd.print(timeClient.getFormattedTime());
        delay(100);
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
void printWeather() {
    // Clear the LCD and informs the weather is being fetched so the user knows
    // the button press was recognized
    lcd.clear();
    lcd.print("Consultando");
    lcd.setCursor(0,1);
    lcd.print("o tempo...");
    
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(client, weatherUrl);

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) { // HTTP 200
            String payload = http.getString();
            Serial.println("Tempo: " + payload);
            payload.replace("°", String((char)223)); // Changes the degree symbol to something the LCD can display
            lcd.clear();
            lcd.print(payload.substring(0, 16)); // Breaks the answer to the size of the LCD
            lcd.setCursor(0, 1);
            lcd.print(payload.substring(16));
            delay(15000);
        } else {
            Serial.println("Falha ao obter dados.");
            Serial.print(http.getString());
        }
        http.end();
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
    timeClient.update();
    if (!timeClient.isTimeSet()) {
        Serial.println("Erro ao atualizar o tempo.");
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
    
    // Uncomment the following lines to get the value of the analog pin
    // into the serial monitor, to adjust the function button
    // int x = analogRead(BUTTON);
    // Serial.print(x);

    // Reads the buttons and take action if some is pressed
    int buttonState = button(analogRead(BUTTON));
    if (buttonState == 1) {
        Serial.println("Select");
    }
    else if (buttonState == 2) {
        Serial.println("Left");
        printNetwork();
        lastMinutes = 0;
    }
    else if (buttonState == 3) {
        Serial.println("Down");
        printNTP();
        lastMinutes = 0;
    }
    else if (buttonState == 4) {
        Serial.println("Up");
        printDate();
        lastMinutes = 0;
    }
    else if (buttonState == 5) {
        Serial.println("Right");
        printWeather();
        lastMinutes = 0;
    }
    else { // No button is pressed
        if (hours != lastHours || minutes != lastMinutes) { // updates the LCD only if the time have changed
            printTime(hours, minutes);
            lastHours = hours;
            lastMinutes = minutes;
        }
    }

    // Makes the : blink on the clock
    delay(100);
    countBlink++;
    if (countBlink == 5) {
        lcd.setCursor(7, 0);
        lcd.print(":");
        lcd.setCursor(7, 1);
        lcd.print(":");
    } else if (countBlink == 10) {
        lcd.setCursor(7, 0);
        lcd.print(" ");
        lcd.setCursor(7, 1);
        lcd.print(" ");
        countBlink = 0;
    }
}
