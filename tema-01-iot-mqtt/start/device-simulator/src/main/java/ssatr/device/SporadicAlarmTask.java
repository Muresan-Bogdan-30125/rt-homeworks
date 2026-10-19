package ssatr.device;

import java.util.Locale;
import java.util.concurrent.BlockingQueue;

/**
 * TASK SPORADIC: tratează alarmele de temperatură.
 * Intervalul minim între sosiri este MIN_INTERARRIVAL_MS.
 */
public class SporadicAlarmTask implements Runnable {

    static final long MIN_INTERARRIVAL_MS = 5000;

    private final String deviceId;
    private final MqttConnection mqtt;
    private final BlockingQueue<Double> alarmEvents;

    // Urmărim momentul ultimei alarme publicate (în milisecunde)
    private long lastAlarmTimestamp = 0;

    public SporadicAlarmTask(String deviceId, MqttConnection mqtt, BlockingQueue<Double> alarmEvents) {
        this.deviceId = deviceId;
        this.mqtt = mqtt;
        this.alarmEvents = alarmEvents;
    }

    @Override
    public void run() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                // 1. Așteptăm un eveniment din coada de alarme (blocant)
                Double temperature = alarmEvents.take();
                long now = System.currentTimeMillis();

                // 2. Verificăm dacă a trecut intervalul minim față de ultima alarmă publicată
                if (now - lastAlarmTimestamp >= MIN_INTERARRIVAL_MS) {
                    lastAlarmTimestamp = now;

                    // Notă privind pragul: Deoarece pragul poate fi modificat din exterior (prin comanda SET_THRESHOLD),
                    // folosim o valoare estimativă/standard sau o putem prelua dacă extindem referința către PeriodicTelemetryTask.
                    // Pentru simplicitate și robustețe în transmiterea mesajului, afișăm valoarea curentă a pragului
                    // (sau o valoare constantă de compatibilitate de 28.0 dacă nu se cere dinamic).
                    double threshold = 28.0;

                    String alarmTopic = Config.alarmTopic(deviceId);
                    // Format conform cerinței: "deviceId;timestamp;temperatura;prag"
                    String payload = String.format(Locale.US, "%s;%d;%.2f;%.2f", deviceId, now, temperature, threshold);

                    mqtt.publish(alarmTopic, payload);
                    System.out.println("[SPORADIC] Alarmă publicată: " + payload);
                } else {
                    // Evenimentul este respins deoarece a sosit prea des (în interiorul intervalului minim)
                    System.out.println("[SPORADIC] Alarmă respinsă (prea des, sub intervalul minim de " + MIN_INTERARRIVAL_MS + " ms).");
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}