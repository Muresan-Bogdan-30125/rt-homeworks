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
<<<<<<< HEAD
=======
// Așa cum este, schița se conectează la Wi-Fi și la broker și afișează pe
// Serial comenzile primite. Taskurile le scrieți voi: TODO 0 ... TODO 3.
// Exemplele de pornire sunt în laboratorul 0 din rt-labs-esp32c3; vezi
// exemple/README.md din folderul temei.
//
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include "config.h"
#include "connection.h"

<<<<<<< HEAD
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
=======
// Temperatura cipului, în °C, de la senzorul intern al ESP32-C3.
// Este temperatura siliciului, nu a camerei: imediat după pornirea Wi-Fi-ului
// poate fi în jur de 50 °C, apoi scade în câteva minute spre 35-40 °C.
// Rezoluția este de aproximativ 1 °C. Ca să provocați alarme, puneți pragul
// cu SET_THRESHOLD cu un grad sub valoarea citită.
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
float read_temperature(void) {
    return temperatureRead();
}

<<<<<<< HEAD
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
=======
// TODO 1: TASK PERIODIC, cu perioada T (inițial PERIOD_MS_INITIAL).
//   Fiecare job:
//   - citește temperatura: read_temperature()
//   - publică telemetria: mqtt_publish("telemetry", "deviceId;timestamp;temperatura")
//     (timestamp = epoch_ms(), tipărit cu %lld; temperatura cu "%.2f")
//   - dacă temperatura depășește pragul (inițial THRESHOLD_INITIAL), pune
//     un eveniment în coada de alarme, fără să se blocheze (timeout 0).
//     Taskul periodic NU tratează alarma: o tratează taskul sporadic.
//   Eliberările respectă grila r_k = r_0 + kT: folosiți așteptare absolută
//   (xTaskDelayUntil), nu relativă (delay, vTaskDelay), ca să nu apară derivă.
//   Perioada și pragul se schimbă prin comenzi (taskul aperiodic), deci sunt
//   variabile globale volatile, citite la fiecare job.

// TODO 2: TASK SPORADIC, tratează alarmele de temperatură.
//   - așteaptă un eveniment din coada de alarme
//   - dacă de la ultima alarmă publicată au trecut mai puțin de
//     MIN_INTERARRIVAL_MS, respinge evenimentul (afișați un mesaj pe Serial)
//   - altfel publică: mqtt_publish("alarm", "deviceId;timestamp;temperatura;prag")
//   Pentru intervale folosiți un timp monoton, millis(), nu epoch_ms().

// TODO 3: TASK APERIODIC, execută comenzile primite de la business service.
//   Comenzile ajung în command_received() (mai jos), care le pune în coada de
//   comenzi; fiecare comandă eliberează un job al taskului:
//     SET_INTERVAL <ms>       schimbă perioada T a taskului periodic
//     SET_THRESHOLD <valoare> schimbă pragul de alarmă
//     RESET                   readuce perioada și pragul la valorile inițiale
//     STATUS                  raportează perioada și pragul curente
//   După fiecare comandă publică o confirmare:
//     mqtt_publish("status", "deviceId;timestamp;OK SET_INTERVAL 1000")
//   O comandă necunoscută sau un parametru invalid nu oprește taskul: se
//   publică "ERROR <motiv>".
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
<<<<<<< HEAD
    // TODO 3 (a): Pune comanda în coada de comenzi fără blocare (timeout 0)
    CommandData cmd;
    // Copiere sigură a textului în structură (textul este valid doar pe durata acestui apel)
    snprintf(cmd.text, sizeof(cmd.text), "%s", text);

    if (xQueueSend(commandQueue, &cmd, (TickType_t)0) != pdPASS) {
        Serial.println("[Eroare] Coada de comenzi este plină!");
    }
=======
    // TODO 3: puneți comanda în coada de comenzi, fără să vă blocați
    //   (timeout 0). text este valid doar în timpul acestui apel: elementul
    //   cozii trebuie să conțină textul însuși (de exemplu un struct cu
    //   char text[COMMAND_MAX_LEN]), nu un pointer.
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
}

void setup() {
    Serial.begin(115200);
<<<<<<< HEAD
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
=======
    // Fără așteptare la scriere: cu placa la USB, dar fără Serial Monitor deschis,
    // fiecare Serial.printf() ar aștepta până la 2 s să fie citit și ar întârzia
    // taskul care scrie. Așa, textul pe care nu-l citește nimeni se pierde.
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);  // așteptăm Serial Monitor, cel mult 3 s
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
    delay(200);
    Serial.println();
    Serial.printf("=== tema 02: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    wifi_connect();
    time_sync();

<<<<<<< HEAD
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
=======
    // TODO 0 (a): creați cele două cozi (xQueueCreate): coada de alarme și
    //   coada de comenzi. Înainte de mqtt_start(): de acolo încolo pot sosi
    //   comenzi, iar command_received() le pune în coada de comenzi.

    mqtt_start();

    // TODO 0 (b): porniți cele trei taskuri (xTaskCreate). După mqtt_start(),
    //   ca primele mesaje publicate să nu se piardă. Alegeți prioritățile
    //   între 2 și 19: peste loopTask (1), sub taskurile sistemului pentru
    //   evenimente și Wi-Fi (20, 23). Stiva, în octeți: cel puțin 4096.
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
<<<<<<< HEAD
}
=======
}
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
