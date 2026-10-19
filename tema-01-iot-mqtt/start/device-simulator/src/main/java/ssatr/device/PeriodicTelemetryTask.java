package ssatr.device;

import java.util.Locale;
import java.util.concurrent.BlockingQueue;

/**
 * TASK PERIODIC, cu perioada T = periodMs: fiecare job citește senzorul și
 * publică valoarea pe topic-ul de telemetrie.
 */
public class PeriodicTelemetryTask implements Runnable {

    private final String deviceId;
    private final TemperatureSensor sensor;
    private final MqttConnection mqtt;
    private final BlockingQueue<Double> alarmEvents;

    // volatile: sunt citite aici, dar modificate de taskul aperiodic (alt thread)
    private volatile long periodMs = 2000;
    private volatile double threshold = 28.0;

    public PeriodicTelemetryTask(String deviceId, TemperatureSensor sensor, MqttConnection mqtt,
                                 BlockingQueue<Double> alarmEvents) {
        this.deviceId = deviceId;
        this.sensor = sensor;
        this.mqtt = mqtt;
        this.alarmEvents = alarmEvents;
    }

    @Override
    public void run() {
        // TODO 1: implementați bucla taskului periodic cu așteptare absolută (pe grilă folosind System.nanoTime())
        long nextRelease = System.nanoTime() + periodMs * 1_000_000L;

        try {
            while (!Thread.currentThread().isInterrupted()) {
                long now = System.nanoTime();
                long sleepNs = nextRelease - now;

                if (sleepNs > 0) {
                    long sleepMs = sleepNs / 1_000_000L;
                    int sleepNsRem = (int) (sleepNs % 1_000_000L);
                    Thread.sleep(sleepMs, sleepNsRem);
                }

                // 1. Citim temperatura de la senzor
                double temperature = sensor.read();
                long timestamp = System.currentTimeMillis();

                // 2. Publicăm telemetria pe MQTT (cu formatare corectă pentru punct zecimal)
                String telemetryTopic = Config.telemetryTopic(deviceId);
                String payload = String.format(Locale.US, "%s;%d;%.2f", deviceId, timestamp, temperature);
                mqtt.publish(telemetryTopic, payload);

                // 3. Dacă temperatura depășește pragul, semnalăm evenimentul în coada alarmEvents
                if (temperature > threshold) {
                    alarmEvents.offer(temperature);
                }

                // Actualizăm grila următoare pe baza perioadei curente (care poate fi modificată din exterior)
                long currentPeriodNs = periodMs * 1_000_000L;
                nextRelease += currentPeriodNs;

                // Dacă am rămas în urmă (intervalul a durat mai mult decât perioada), refacem alinierea pe grilă
                if (System.nanoTime() > nextRelease) {
                    nextRelease = System.nanoTime() + currentPeriodNs;
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    public void setPeriodMs(long periodMs) {
        this.periodMs = periodMs;
    }

    public void setThreshold(double threshold) {
        this.threshold = threshold;
    }

    public long getPeriodMs() {
        return periodMs;
    }

    public double getThreshold() {
        return threshold;
    }
}