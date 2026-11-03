// Device-ul IoT din tema 01, de data aceasta pe un ESP32-C3 Super Mini.
//
// Aceeași logică și același protocol ca device-simulator din tema 01, cu trei
// taskuri FreeRTOS în loc de trei thread-uri Java:
//
//   task periodic --(coada de alarme)--> task sporadic
//        |                                     |
//        +---- telemetry ---> MQTT <--- alarm -+
//                              |
//                          commands --> command_received() --(coada de comenzi)--> task aperiodic ---> status
//
// Gata implementate (nu trebuie modificate):
//   config.h         setările: Wi-Fi, broker, STUDENT, valorile inițiale; DEVICE_ID
//                    se citește din adresa MAC a plăcii
//   connection.h/cpp Wi-Fi, ora exactă (SNTP), clientul MQTT, mqtt_publish(), epoch_ms()
//
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include "config.h"
#include "connection.h"

// Variabile globale volatile pentru perioadă și prag (modificabile prin comenzi aperiodice)
volatile unsigned long periodMs = PERIOD_MS_INITIAL;
volatile float threshold = THRESHOLD_INITIAL;

// Handle-uri pentru cozile FreeRTOS
QueueHandle_t alarmQueue = NULL;
QueueHandle_t commandQueue = NULL;

// Structura pentru coada de alarme
struct AlarmData {
    float temperature;
    float threshold;
    unsigned long long timestamp;
};

// Structura pentru coada de comenzi
struct CommandData {
    char text[COMMAND_MAX_LEN];
};

// Temperatura cipului, în °C, de la senzorul intern al ESP32-C3.
float read_temperature(void) {
    return temperatureRead();
}

// Prototipuri taskuri FreeRTOS
void PeriodicTelemetryTask(void *pvParameters);
void SporadicAlarmTask(void *pvParameters);
void AperiodicCommandTask(void *pvParameters);

// ==========================================
// TODO 1: TASK PERIODIC
// ==========================================
void PeriodicTelemetryTask(void *pvParameters) {
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // 1. Citește temperatura internă și timestamp-ul SNTP
        float temp = read_temperature();
        unsigned long long ts = epoch_ms();

        // 2. Publică telemetria: format "deviceId;timestamp;temperatura"
        char telemetryMsg[128];
        snprintf(telemetryMsg, sizeof(telemetryMsg), "%s;%lld;%.2f", DEVICE_ID, ts, temp);
        mqtt_publish("telemetry", telemetryMsg);
        Serial.printf("[Periodic] Telemetrie trimisă: %s\n", telemetryMsg);

        // 3. Verifică pragul pentru alarmă
        if (temp > threshold) {
            AlarmData alarm;
            alarm.temperature = temp;
            alarm.threshold = threshold;
            alarm.timestamp = ts;

            // Trimite în coada de alarme fără blocare (timeout 0)
            if (xQueueSend(alarmQueue, &alarm, (TickType_t)0) != pdPASS) {
                Serial.println("[Periodic] Avertisment: Coada de alarme este plină!");
            } else {
                Serial.println("[Periodic] Prag depășit. Alarmă trimisă în coadă.");
            }
        }

        // 4. Așteptare absolută pe grilă folosind perioada curentă
        TickType_t xFrequency = pdMS_TO_TICKS(periodMs);
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ==========================================
// TODO 2: TASK SPORADIC
// ==========================================
void SporadicAlarmTask(void *pvParameters) {
    AlarmData receivedAlarm;
    unsigned long long lastAlarmTimestamp = 0;

    for (;;) {
        // Așteaptă un element din coada de alarme (blocant pe termen nelimitat)
        if (xQueueReceive(alarmQueue, &receivedAlarm, portMAX_DELAY) == pdPASS) {
            unsigned long currentTimeMillis = millis(); // Timp monoton pentru filtrul temporal

            // Verifică intervalul minim între sosiri (MIN_INTERARRIVAL_MS)
            if (lastAlarmTimestamp == 0 || (currentTimeMillis - lastAlarmTimestamp) >= MIN_INTERARRIVAL_MS) {
                char alarmMsg[128];
                snprintf(alarmMsg, sizeof(alarmMsg), "%s;%lld;%.2f;%.2f",
                         DEVICE_ID, receivedAlarm.timestamp, receivedAlarm.temperature, receivedAlarm.threshold);

                mqtt_publish("alarm", alarmMsg);
                Serial.printf("[Sporadic] Alarmă publicată: %s\n", alarmMsg);

                lastAlarmTimestamp = currentTimeMillis;
            } else {
                Serial.println("[Sporadic] Alarmă respinsă (sub intervalul minim MIN_INTERARRIVAL_MS).");
            }
        }
    }
}

// ==========================================
// TODO 3 (b): TASK APERIODIC (Execuție comenzi)
// ==========================================
void AperiodicCommandTask(void *pvParameters) {
    CommandData cmd;

    for (;;) {
        if (xQueueReceive(commandQueue, &cmd.text, portMAX_DELAY) == pdPASS) {
            char responseStatus[128];
            unsigned long long ts = epoch_ms();

            // Parsare comenzi
            if (strncmp(cmd.text, "SET_INTERVAL", 12) == 0) {
                unsigned long newPeriod = 0;
                if (sscanf(cmd.text, "SET_INTERVAL %lu", &newPeriod) == 1 && newPeriod > 0) {
                    periodMs = newPeriod;
                    snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;OK SET_INTERVAL %lu", DEVICE_ID, ts, periodMs);
                } else {
                    snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;ERROR Argument invalid pentru SET_INTERVAL", DEVICE_ID, ts);
                }
            }
            else if (strncmp(cmd.text, "SET_THRESHOLD", 13) == 0) {
                float newThreshold = 0;
                if (sscanf(cmd.text, "SET_THRESHOLD %f", &newThreshold) == 1) {
                    threshold = newThreshold;
                    snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;OK SET_THRESHOLD %.2f", DEVICE_ID, ts, threshold);
                } else {
                    snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;ERROR Argument invalid pentru SET_THRESHOLD", DEVICE_ID, ts);
                }
            }
            else if (strcmp(cmd.text, "RESET") == 0) {
                periodMs = PERIOD_MS_INITIAL;
                threshold = THRESHOLD_INITIAL;
                snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;OK RESET", DEVICE_ID, ts);
            }
            else if (strcmp(cmd.text, "STATUS") == 0) {
                snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;OK STATUS T=%lu TH=%.2f", DEVICE_ID, ts, periodMs, threshold);
            }
            else {
                snprintf(responseStatus, sizeof(responseStatus), "%s;%lld;ERROR Comanda necunoscuta", DEVICE_ID, ts);
            }

            // Publică starea/confirmarea pe topicul status
            mqtt_publish("status", responseStatus);
            Serial.printf("[Aperiodic] Comandă executată, status publicat: %s\n", responseStatus);
        }
    }
}

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
    // TODO 3 (a): Pune comanda în coada de comenzi fără blocare (timeout 0)
    CommandData cmd;
    // Copiere sigură a textului în structură (textul este valid doar pe durata acestui apel)
    snprintf(cmd.text, sizeof(cmd.text), "%s", text);

    if (xQueueSend(commandQueue, &cmd, (TickType_t)0) != pdPASS) {
        Serial.println("[Eroare] Coada de comenzi este plină!");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(200);
    Serial.println();
    Serial.printf("=== tema 02: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    wifi_connect();
    time_sync();

    // ==========================================
    // TODO 0 (a): Crearea cozilor înainte de mqtt_start()
    // ==========================================
    alarmQueue = xQueueCreate(5, sizeof(AlarmData));
    commandQueue = xQueueCreate(5, sizeof(CommandData));

    if (alarmQueue == NULL || commandQueue == NULL) {
        Serial.println("[Eroare fatala] Nu s-au putut crea cozile FreeRTOS!");
        return;
    }

    mqtt_start();

    // ==========================================
    // TODO 0 (b): Pornirea taskurilor după mqtt_start()
    // Priorități între 2 și 19, stivă 4096 octeți
    // ==========================================
    xTaskCreate(PeriodicTelemetryTask, "PeriodicTask", 4096, NULL, 4, NULL);
    xTaskCreate(SporadicAlarmTask,     "SporadicTask", 4096, NULL, 3, NULL);
    xTaskCreate(AperiodicCommandTask,  "AperiodicTask", 4096, NULL, 2, NULL);

    Serial.println("[Init] Toate taskurile și cozile au pornit cu succes!");
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
}