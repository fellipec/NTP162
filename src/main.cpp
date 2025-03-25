#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LiquidCrystal.h>
#include <wifi_credentials.h>


// The correct sequence of pins Wemos D1 similar to Arduino UNO
// link D-pins to GPIO for version R1
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

LiquidCrystal lcd(D8, D9, D4, D5, D6, D7);
#include <digits.h>

// Lista de servidores NTP
const char* ntpServers[] = {
    "192.168.100.224", 
    "scarlett", // Servidor NTP local
    "a.ntp.br","b.ntp.br","c.ntp.br", // Servidores NTP brasileiros
    "time.nist.gov", // Servidor NTP dos EUA
    "pool.ntp.org" // Pool de servidores NTP
};

int numRedes = sizeof(ssids) / sizeof(ssids[0]);  // Número de redes na lista
int numNTPServers = sizeof(ntpServers) / sizeof(ntpServers[0]); // Número de servidores NTP na lista
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[0], -3 * 3600, 60000); // UTC-3 (Brasil)


int counter = 0;
int buttonState = 0;
const char* gizmo[] = {"|", ">", "=", "<"};
// Dias da semana
const char* daysOfTheWeek[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};

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


void setup()
{
    // pinMode(BUTTON, INPUT);
    Serial.begin(115200);
    lcd.begin(16,2); // Inicializa o LCD com 16 colunas e 2 linhas
    lcd.clear();
    lcd.print("Conectando em:");
    bool conectado = false;
    // Loop para tentar conectar a cada SSID na lista
    for (int i = 0; i < numRedes; i++) {
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        lcd.setCursor(0, 1);
        lcd.print(ssids[i]);
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        // Tentativas de conexão até o Wi-Fi se conectar ou falhar após 10 segundos
        while (WiFi.status() != WL_CONNECTED && tentativa < 100) {
            delay(100);
            Serial.print(".");
            lcd.setCursor(15,1);
            lcd.print(gizmo[i]);
            i = (i + 1) % 4; // Cicla entre 0 e 3
            tentativa++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConectado!");
            lcd.clear();
            lcd.print("Conectado ao ");
            lcd.setCursor(0, 1);
            lcd.print("Wi-Fi: ");
            lcd.print(ssids[i]);
            conectado = true;
            break; // Sai do loop se a conexão for bem-sucedida
        } else {
            Serial.println("\nFalha ao conectar.");
        }
    }

    // Se nenhuma conexão for bem-sucedida
    if (!conectado) {
        lcd.clear();
        lcd.print("Erro ao conectar");
        delay(10000);
        ESP.restart();
    }
    lcd.clear();
    int n = tryNTPServer();
    if (n >= 0) {
        lcd.print("Conectado ao NTP");
        lcd.setCursor(0, 1);
        lcd.print(ntpServers[n]);
        delay(2000);
    } else {
        lcd.print("Erro ao conectar NTP");
        delay(10000);
        ESP.restart();
    }
    
  
    lcd.createChar(0,LT);
    lcd.createChar(1,UB);
    lcd.createChar(2,RT);
    lcd.createChar(3,LL);
    lcd.createChar(4,LB);
    lcd.createChar(5,LR);
    lcd.createChar(6,MB);
    lcd.createChar(7,block);
    // Print a message to the LCD.
    lcd.backlight();
    
    lcd.clear();
    printDigits(0,0);
    printDigits(0,4);
    printDigits(0,8);
    printDigits(0,12);
    delay(1000);
}


void printNumber(int val){
     int col=9;    
     printDigits(val/10,col);
     printDigits(val%10,col+4);
}



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

void printDate() {
    timeClient.update();
    unsigned long epoch = timeClient.getEpochTime();
    
    // Calculando a data (dia, mês, ano)
    int seconds = epoch % 60;
    int minutes = (epoch / 60) % 60;
    int hours = (epoch / 3600) % 24;
    int days = epoch / 86400;
    
    // Definir a data de início para o UNIX epoch: 01/01/1970
    int year = 1970;
    int month = 1;
    int day = 1;

    // Calcular o número de anos passados
    while (days >= (365 + (year % 4 == 0 ? 1 : 0))) {
        days -= (365 + (year % 4 == 0 ? 1 : 0));
        year++;
    }

    // Calcular o número do mês
    int daysInMonth[] = {31, (year % 4 == 0 ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while (days >= daysInMonth[month - 1]) {
        days -= daysInMonth[month - 1];
        month++;
    }

    day += days;

    // Exibindo a data
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Data:");
    lcd.setCursor(0, 1);
    lcd.print(daysOfTheWeek[timeClient.getDay()]);
    lcd.print(" ");
    lcd.print(day);
    lcd.print("/");
    lcd.print(month);
    lcd.print("/");
    lcd.print(year);
    delay(5000);
}

void printNetwork() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 1);
    lcd.print(WiFi.SSID());
    delay(5000);
}


int botao(int valorAnalogico) {
    if (valorAnalogico > 1010) {
        return 0;
    } else if (valorAnalogico > 900) {
        return 1; // Select
    } else if (valorAnalogico > 600) {
        return 2; // Left
    } else if (valorAnalogico > 300) {
        return 3; // Down
    } else if (valorAnalogico > 100) {
        return 4; // Up
    } else if (valorAnalogico >= 0) {
        return 5; // Right
    }
    return 0;
}

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
    

    // int x = analogRead(BUTTON);
    // Serial.print(x);
    int buttonState = botao(analogRead(BUTTON));
    if (buttonState == 1) {
        printDate();
        Serial.println("Select");
    }
    else if (buttonState == 2) {
        printNetwork();
        Serial.println("Left");
    }
    else if (buttonState == 3) {
        Serial.println("Down");
    }
    else if (buttonState == 4) {
        Serial.println("Up");
    }
    else if (buttonState == 5) {
        Serial.println("Right");
    }
    else {
        printTime(hours, minutes);
        // Serial.println("Nenhum botao pressionado");
    }

    delay(1000);
}
