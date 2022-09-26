#include <WiFi.h>
#include <ThingSpeak.h>
#include <HTTPClient.h>
#include <time.h>

// WiFi network credentials
const char *ssid = "esw-m19@iiith";
const char *password = "e5W-eMai@3!20hOct";

// ThingSpeak Channel
const unsigned long CHANNEL_NO = 1848077;
const char *API_KEY = "VC7BK32B18MX4VZP";

const int currentSensor = A0;
const int REPS = 1000;

const time_t updateInterval = 50000;
const time_t connectionDelay = 1000;
const time_t rmsValueInterval = 5000;
const time_t sensorReadInterval = 5;

time_t lastUpdated = 0;
time_t lastRmsRead = -rmsValueInterval;
time_t lastSensorRead = -sensorReadInterval;
time_t lastConnected = -connectionDelay;

time_t Epoch_Time;

WiFiClient client;
const char *ntpServer = "pool.ntp.org";
const String oneM2M_server = "http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-36/Node-1/Data/KCIS-3";
String oneM2M_msg;

const double V0G = 1.43;
float V_rms, I_rms;

// Get_Epoch_Time() Function that gets current epoch time
time_t Get_Epoch_Time()
{
	time_t now;
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo))
	{
		// Serial.println("Failed to obtain time");
		return (0);
	}
	time(&now);
	return now;
}

void connectWiFi()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.println("Attempting to connect. . .");
		while (WiFi.status() != WL_CONNECTED)
		{
			WiFi.begin(ssid, password);
			while (millis() - lastConnected <= connectionDelay)
				;
			lastConnected = millis();
			Serial.print(" .");
		}
		Serial.println("Connected.");
	}
}

void createCI(String &val)
{
	HTTPClient http;
	http.begin(oneM2M_server);
	http.addHeader("X-M2M-Origin", "l5jvey:zHDyFr");
	http.addHeader("Content-Type", "application/json;ty=4");
	int code = http.POST("{\"m2m:cin\": {\"con\": \"" + val + "\"}}");
	Serial.println(code);
	if (code == -1)
	{
		Serial.println("UNABLE TO CONNECT TO THE SERVER");
	}
	else if (code == 201)
	{
		Serial.println("Updated");
	}
	else
	{
		Serial.println("Error!!!");
	}
	http.end();
}

void printV0G()
{
	for (int _ = 0; _ < 3; ++_)
	{
		float V0G_RMS = 0;
		for (int i = 0; i < REPS; ++i)
		{
			float V0G1 = (analogRead(currentSensor) * (float)3.3) / (float)4095.0;
			if (i % 100 == 0)
			{
				Serial.print("V0G = ");
				Serial.print(V0G1);
				Serial.print(" V\n");
			}
			V0G_RMS += V0G1;
			delay(10);
		}
		V0G_RMS /= (float)(1.0 * REPS);
		Serial.print("<---- V0G_MEAN = ");
		Serial.print(V0G_RMS);
		Serial.print(" V ---->\n");
	}
}

void getSensorValues(float &V_rms, float &I_rms)
{
	double sensorValue;
	double sqrSum = 0.0, V_out = 0.0, sqrDiff = 0.0;
	Serial.print("-> ");
	for (int i = 0; i < REPS; ++i)
	{
		sensorValue = analogRead(currentSensor);
		V_out = (sensorValue * (double)3.3) / (double)4095.0;
		sqrDiff = abs(V_out - V0G);
		if (i % 200 == 0)
		{
			Serial.print(". ");
		}
		sqrSum += sqrDiff * sqrDiff;
		while (millis() - lastSensorRead <= sensorReadInterval)
			;
		lastSensorRead = millis();
	}
	V_rms = (float)(sqrt(sqrSum / (double)REPS));
	I_rms = V_rms / (float)0.066;
	time_t TimeStamp = Get_Epoch_Time();
	String message = String(TimeStamp) + "," + String(V_rms) + "," + String(I_rms);
	oneM2M_msg += message + ";";
	String SerialMsg = "V_rms = " + String(V_rms) + " V, I_rms = " + String(I_rms) + " A";
	Serial.println(SerialMsg);
}

void setup()
{
	Serial.begin(115200);
	pinMode(currentSensor, INPUT);
	delay(10);
	// printV0G();
	connectWiFi();
	configTime(0, 0, ntpServer);
	ThingSpeak.begin(client);
}

void loop()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		connectWiFi();
	}

	if (millis() - lastRmsRead > rmsValueInterval)
	{
		getSensorValues(V_rms, I_rms);
		lastRmsRead = millis();
	}

	if ((millis() - lastUpdated) > updateInterval)
	{
		oneM2M_msg = oneM2M_msg.substring(0, oneM2M_msg.length() - 1);
		Serial.println(oneM2M_msg);
		createCI(oneM2M_msg);

		ThingSpeak.setField(2, V_rms);
		ThingSpeak.setField(3, I_rms);
		ThingSpeak.writeFields(CHANNEL_NO, API_KEY);

		lastUpdated = millis();
		oneM2M_msg = "";
	}
}