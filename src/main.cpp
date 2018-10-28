#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

/* Definitions */
#define WIFI_SSID ""
#define WIFI_PWD "!"
#define REGION "tenan"
#define COUNTRY "kr"
#define APPID ""

#define TIME_ZONE 9
#define DAYTIME_SAVING 0

/* Weather information structure */
typedef struct
{
	const char *name;
	double temp;
	int humidity;
	int temp_min;
	int temp_max;
	int speed;
	int direction;
	int conditionId;
} _weatherinfo;

_weatherinfo weatherinfo;

/* Variables */
char strToday[8]; /* e.g. 20181212 */
char strNow[19];  /* e.g. 2018.12.12 12:12:12 */

/* Functions */
bool initWiFi(void);
void initTime(void);
bool initSD(void);
void requestWeatherInfo(void);
void parseWeatherJson(String);
void dumpLog(void);

void setup()
{
	Serial.begin(115200);
	//Serial.setDebugOutput(true);

	/* Initialize WiFi */
	if (!initWiFi()) return;
	
	/* Initialize Time */
	initTime();

	/* Initialize SD Card */
	if (!initSD()) return;	
}

void loop()
{
	time_t now = time(nullptr);
	tm *local = localtime(&now);
	sprintf(strToday, "%04d%02d%02d", (local->tm_year + 1900), (local->tm_mon + 1), local->tm_mday);
	sprintf(strNow, "%04d.%02d.%02d %02d:%02d:%02d", (local->tm_year + 1900), (local->tm_mon + 1), local->tm_mday, 
			local->tm_hour, local->tm_min, local->tm_sec);

	requestWeatherInfo();
	dumpLog();

	/* Check RAM */
	Serial.print(F("Free Heap: "));
	Serial.println(ESP.getFreeHeap());

	/* 60 secs */
	delay(60 * 1000);
}

bool initWiFi()
{
	Serial.println();

	if (!WiFi.begin(WIFI_SSID, WIFI_PWD))
	{
		Serial.println("ERROR: WiFi.begin");
		return false;
	}

	Serial.println("OK: WiFi.begin");

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(100);
		Serial.print(".");
	}

	Serial.println();
	Serial.println("OK: WiFi connected");

	delay(1000);

	return true;
}

void initTime()
{
	/* Congiure Time */
	Serial.println("Initialize Time...");

	configTime(TIME_ZONE * 3600, DAYTIME_SAVING, "pool.ntp.org", "time.nist.gov");

	while (!time(nullptr))
	{
		Serial.print(".");
		delay(1000);
	}

	Serial.println("OK: initTime");
}

bool initSD()
{
	Serial.println("Initialize SD card...");

	if (!SD.begin(15))
	{
		Serial.println("ERROR: Initialize SD card...");
		return false;
	}

	Serial.println("OK: Initialize SD Card");

	return true;
}

void requestWeatherInfo()
{
	HTTPClient httpClient;
	httpClient.setTimeout(2000);

	/* Connect & Request */
	String url = String("/data/2.5/weather?q=") + String(REGION) + String(",") + String(COUNTRY) + String("&units=metric&appid=") + String(APPID);
	if (!httpClient.begin("api.openweathermap.org", 80, url.c_str()))
	{
		Serial.println("ERROR: HTTPClient.begin");
		return;
	}

	Serial.println("OK: HTTPClient.begin");

	int httpCode = httpClient.GET();

	/* Check response */
	if (httpCode > 0)
	{
		Serial.printf("[HTTP] request from the client was handled: %d\n", httpCode);
		String payload = httpClient.getString();
		parseWeatherJson(payload);
	}
	else
	{
		Serial.printf("[HTTP] connection failed: %s\n", httpClient.errorToString(httpCode).c_str());
	}

	httpClient.end();
}

void parseWeatherJson(String buffer)
{
	int JsonStartIndex = buffer.indexOf('{');
	int JsonLastIndex = buffer.lastIndexOf('}');

	/* Substring JSON string */
	String JsonStr = buffer.substring(JsonStartIndex, JsonLastIndex + 1);
	Serial.println("PARSE JSON WEATHER INFORMATION: " + JsonStr);

	/* Parse JSON string */
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.parseObject(JsonStr);

	if (root.success())
	{
		/* Get information */
		weatherinfo.temp = root["main"]["temp"];
		weatherinfo.humidity = root["main"]["humidity"];
		weatherinfo.temp_min = root["main"]["temp_min"];
		weatherinfo.temp_max = root["main"]["temp_max"];
		weatherinfo.speed = root["wind"]["speed"];
		weatherinfo.direction = root["wind"]["direction"];
		weatherinfo.conditionId = root["weather"][0]["id"];
		weatherinfo.name = root["name"];

		/* Serial Output */
		Serial.printf("Name: %s\r\n", weatherinfo.name);
		Serial.printf("Temp: %3.1f\r\n", weatherinfo.temp);
		Serial.printf("Humidity: %d\r\n", weatherinfo.humidity);
		Serial.printf("Min. Temp: %d\r\n", weatherinfo.temp_min);
		Serial.printf("Max. Temp: %d\r\n", weatherinfo.temp_max);
		Serial.printf("Wind Speed: %d\r\n", weatherinfo.speed);
		Serial.printf("Wind Direction: %d\r\n", weatherinfo.direction);
		Serial.printf("ConditionId: %d\r\n", weatherinfo.conditionId);
	}
	else
	{
		Serial.println("jsonBuffer.parseObject failed");
	}
}

void dumpLog()
{
	String logFileName = String(strToday) + ".txt";
	File logFile = SD.open(logFileName, FILE_WRITE);
	if (logFile)
	{
		Serial.println("OK: Open log File " + logFileName);
		logFile.printf("Time:%s, Name:%s, Temp:%3.1f, Humdity:%d, Min.Temp:%d, Max.Temp:%d, Wind Speed:%d, Wind Direction:%d, ConditionId:%d\r\n",
					   strNow, weatherinfo.name, weatherinfo.temp, weatherinfo.humidity, weatherinfo.temp_min,
					   weatherinfo.temp_max, weatherinfo.speed, weatherinfo.direction, weatherinfo.conditionId);

		Serial.println("OK: Write logs");

		logFile.close();
		Serial.println("OK: Close the log file");
	}
}
