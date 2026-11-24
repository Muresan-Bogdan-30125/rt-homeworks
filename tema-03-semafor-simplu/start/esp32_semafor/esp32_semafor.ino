// Semafor pentru pietoni, cu buton, pe ESP32-C3 Super Mini.
//
// Placa este controlerul semaforului; lămpile sunt desenate de dashboard, care
// le aprinde după mesajele "lights" publicate de placă. Patru activități:
//
//   ISR buton BOOT ----+
//                      +--(coada butonului)--> task sporadic
//   command_received --+   (BUTTON)                 |
//                                                   v
//                                         [ starea semaforului ] <-- mutex
//                                            ^          |
//                                            |          v
//                         task periodic de control   task periodic de ieșire
//                         (fazele, T = 100 ms)       (lămpile, T = 500 ms) --> lights
//
// Gata implementate (nu trebuie modificate):
//   config.h         setările: Wi-Fi, broker, STUDENT, pinii, perioadele, duratele fazelor
//   connection.h/cpp Wi-Fi, ora exactă (SNTP), clientul MQTT, mqtt_publish(), epoch_ms()
//
// Așa cum este, schița se conectează la Wi-Fi și la broker și afișează pe
// Serial comenzile primite. Restul îl scrieți voi: TODO 0 ... TODO 3.
//
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include "config.h"
#include "connection.h"

// Fazele semaforului (vezi tabelul și diagrama din README.md). Numele din
// PHASE_NAMES sunt cele din mesajul "lights", în aceeași ordine ca în enum.
enum Phase { CAR_GREEN, CAR_YELLOW, ALL_RED, PED_GREEN };
const char* const PHASE_NAMES[] = {"CAR_GREEN", "CAR_YELLOW", "ALL_RED", "PED_GREEN"};

// Culorile unei lămpi, cu numele din mesajul "lights".
enum Light { RED, YELLOW, GREEN };
const char* const LIGHT_NAMES[] = {"RED", "YELLOW", "GREEN"};

// TODO 0 (c): STAREA SEMAFORULUI, partajată de taskuri și protejată de un
//   mutex (xSemaphoreCreateMutex). De exemplu un struct cu:
//   - faza curentă
//   - momentul intrării în faza curentă (millis()), din care se calculează
//     cât a durat faza și cât mai are
//   - cererea pietonului (lampa AȘTEPTAȚI): setată de taskul sporadic,
//     ștearsă de taskul de control la intrarea în PED_GREEN
struct TrafficState {
    Phase phase;
    uint32_t phaseEnteredMs;
    bool pedRequested;
};

struct ButtonEvent {
    uint32_t pressMs;
};

static TrafficState g_state = { CAR_GREEN, 0, false };
static SemaphoreHandle_t g_stateMutex = NULL;
static QueueHandle_t g_buttonQueue = NULL;

// TODO 1: TASK PERIODIC DE CONTROL, cu perioada CONTROL_PERIOD_MS.
void ControlTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t periodTicks = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        uint32_t nowMs = millis();
        bool changed = false;
        Phase nextPhase = CAR_GREEN;

        if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
            uint32_t elapsed = nowMs - g_state.phaseEnteredMs;

            switch (g_state.phase) {
                case CAR_GREEN:
                    if (g_state.pedRequested && elapsed >= CAR_GREEN_MIN_MS) {
                        g_state.phase = CAR_YELLOW;
                        g_state.phaseEnteredMs = nowMs;
                        changed = true;
                        nextPhase = g_state.phase;
                    }
                    break;

                case CAR_YELLOW:
                    if (elapsed >= CAR_YELLOW_MS) {
                        g_state.phase = ALL_RED;
                        g_state.phaseEnteredMs = nowMs;
                        changed = true;
                        nextPhase = g_state.phase;
                    }
                    break;

                case ALL_RED:
                    if (elapsed >= ALL_RED_MS) {
                        g_state.phase = PED_GREEN;
                        g_state.phaseEnteredMs = nowMs;
                        g_state.pedRequested = false;
                        changed = true;
                        nextPhase = g_state.phase;
                    }
                    break;

                case PED_GREEN:
                    if (elapsed >= PED_GREEN_MS) {
                        g_state.phase = CAR_GREEN;
                        g_state.phaseEnteredMs = nowMs;
                        changed = true;
                        nextPhase = g_state.phase;
                    }
                    break;
            }

            xSemaphoreGive(g_stateMutex);
        }

        if (changed) {
            Serial.printf("[control] faza noua: %s\n", PHASE_NAMES[nextPhase]);
        }

        xTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}

// TODO 2: TASK PERIODIC DE IEȘIRE, cu perioada OUTPUT_PERIOD_MS.
void OutputTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t periodTicks = pdMS_TO_TICKS(OUTPUT_PERIOD_MS);

    for (;;) {
        TrafficState snapshot;
        uint32_t nowMs = millis();

        if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
            snapshot = g_state;
            xSemaphoreGive(g_stateMutex);
        } else {
            xTaskDelayUntil(&lastWakeTime, periodTicks);
            continue;
        }

        Light carLight = RED;
        Light pedLight = RED;
        int remainingSec = 0;

        switch (snapshot.phase) {
            case CAR_GREEN:
                carLight = GREEN;
                pedLight = RED;
                remainingSec = 0;
                break;

            case CAR_YELLOW:
                carLight = YELLOW;
                pedLight = RED;
                remainingSec = 0;
                break;

            case ALL_RED:
                carLight = RED;
                pedLight = RED;
                remainingSec = 0;
                break;

            case PED_GREEN: {
                carLight = RED;
                pedLight = GREEN;
                uint32_t elapsed = nowMs - snapshot.phaseEnteredMs;
                if (elapsed < PED_GREEN_MS) {
                    uint32_t remMs = PED_GREEN_MS - elapsed;
                    remainingSec = (int)((remMs + 999UL) / 1000UL);
                } else {
                    remainingSec = 0;
                }
                break;
            }
        }

        int waitLamp = snapshot.pedRequested ? 1 : 0;
        digitalWrite(LED_PIN, snapshot.pedRequested ? LOW : HIGH);

        unsigned long long ts = epoch_ms();
        char payload[160];
        snprintf(
            payload,
            sizeof(payload),
            "%s;%llu;%s;%s;%s;%d;%d",
            DEVICE_ID,
            ts,
            PHASE_NAMES[snapshot.phase],
            LIGHT_NAMES[carLight],
            LIGHT_NAMES[pedLight],
            waitLamp,
            remainingSec
        );

        mqtt_publish("lights", payload);
        xTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}

// TODO 3: TASK SPORADIC, tratează apăsările butonului pentru pietoni.
void SporadicButtonTask(void *pvParameters) {
    (void)pvParameters;
    ButtonEvent ev;
    bool hasAccepted = false;
    uint32_t lastAcceptedMs = 0;

    for (;;) {
        if (xQueueReceive(g_buttonQueue, &ev, portMAX_DELAY) == pdTRUE) {
            if (hasAccepted && ((ev.pressMs - lastAcceptedMs) < MIN_INTERARRIVAL_MS)) {
                Serial.println("[sporadic] apasare respinsa (prea rapida)");
                continue;
            }

            hasAccepted = true;
            lastAcceptedMs = ev.pressMs;

            if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
                g_state.pedRequested = true;
                xSemaphoreGive(g_stateMutex);
            }

            Serial.println("[sporadic] cerere pieton acceptata");
        }
    }
}

// TODO 3: RUTINA DE ÎNTRERUPERE a butonului BOOT
void IRAM_ATTR button_isr(void) {
    ButtonEvent ev;
    ev.pressMs = millis();

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(g_buttonQueue, &ev, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
    // TODO 3: "BUTTON" merge în coada butonului (o apăsare, la fel ca cea din
    //   ISR), fără să vă blocați (timeout 0). Celelalte comenzi se ignoră:
    //   în această temă placa nu are alte comenzi.
    if (strcmp(text, "BUTTON") == 0 && g_buttonQueue != NULL) {
        ButtonEvent ev;
        ev.pressMs = millis();
        if (xQueueSend(g_buttonQueue, &ev, 0) != pdTRUE) {
            Serial.println("[mqtt] coada butonului plina");
        }
    }
}

void setup() {
    Serial.begin(115200);
    // Fără așteptare la scriere: cu placa la USB, dar fără Serial Monitor deschis,
    // fiecare Serial.printf() ar aștepta până la 2 s să fie citit și ar întârzia
    // taskul care scrie. Așa, textul pe care nu-l citește nimeni se pierde.
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);  // așteptăm Serial Monitor, cel mult 3 s
    delay(200);
    Serial.println();
    Serial.printf("=== tema 03: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // LED stins
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    wifi_connect();
    time_sync();

    // TODO 0 (a): creați mutexul stării (xSemaphoreCreateMutex) și coada
    //   butonului (xQueueCreate). Înainte de mqtt_start(): de acolo încolo pot
    //   sosi comenzi, iar command_received() pune "BUTTON" în coadă.
    //   Notați și momentul intrării în faza inițială (CAR_GREEN).
    g_stateMutex = xSemaphoreCreateMutex();
    g_buttonQueue = xQueueCreate(10, sizeof(ButtonEvent));

    if (g_stateMutex == NULL || g_buttonQueue == NULL) {
        Serial.println("[init] eroare la crearea mutexului sau cozii");
        while (true) delay(1000);
    }

    if (xSemaphoreTake(g_stateMutex, portMAX_DELAY) == pdTRUE) {
        g_state.phase = CAR_GREEN;
        g_state.phaseEnteredMs = millis();
        g_state.pedRequested = false;
        xSemaphoreGive(g_stateMutex);
    }

    mqtt_start();

    // TODO 0 (b): porniți cele trei taskuri (xTaskCreate), apoi înregistrați
    //   întreruperea butonului (attachInterrupt). Alegeți prioritățile între
    //   2 și 19 (peste loopTask, sub taskurile sistemului); pentru cele două
    //   taskuri periodice, după regula Rate Monotonic. Stiva, în octeți: cel puțin 4096.
    xTaskCreate(ControlTask,        "ControlTask",  4096, NULL, 5, NULL);
    xTaskCreate(SporadicButtonTask, "SporadicBtn",  4096, NULL, 4, NULL);
    xTaskCreate(OutputTask,         "OutputTask",   4096, NULL, 3, NULL);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, FALLING);
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
}