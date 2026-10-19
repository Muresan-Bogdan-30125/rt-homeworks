package ssatr.device;

import java.util.concurrent.BlockingQueue;

/**
 * TASK APERIODIC: execută comenzile primite de la business service.
 */
public class AperiodicCommandTask implements Runnable {

    private final String deviceId;
    private final TemperatureSensor sensor;
    private final MqttConnection mqtt;
    private final BlockingQueue<String> commands;
    private final PeriodicTelemetryTask periodic;

    public AperiodicCommandTask(String deviceId, TemperatureSensor sensor, MqttConnection mqtt,
                                BlockingQueue<String> commands, PeriodicTelemetryTask periodic) {
        this.deviceId = deviceId;
        this.sensor = sensor;
        this.mqtt = mqtt;
        this.commands = commands;
        this.periodic = periodic;
    }

    @Override
    public void run() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                // Preluăm comanda din coadă (blocant)
                String commandStr = commands.take();
                long timestamp = System.currentTimeMillis();
                String responseText;

                try {
                    String[] parts = commandStr.trim().split("\\s+", 2);
                    String action = parts[0].toUpperCase();
                    String param = parts.length > 1 ? parts[1] : "";

                    switch (action) {
                        case "SET_INTERVAL":
                            if (param.isEmpty()) {
                                throw new IllegalArgumentException("Parametru lipsă pentru SET_INTERVAL");
                            }
                            long newPeriod = Long.parseLong(param);
                            periodic.setPeriodMs(newPeriod);
                            responseText = "OK " + commandStr;
                            break;

                        case "SET_THRESHOLD":
                            if (param.isEmpty()) {
                                throw new IllegalArgumentException("Parametru lipsă pentru SET_THRESHOLD");
                            }
                            double newThreshold = Double.parseDouble(param);
                            periodic.setThreshold(newThreshold);
                            responseText = "OK " + commandStr;
                            break;

                        case "RESET":
                            sensor.reset();
                            responseText = "OK " + commandStr;
                            break;

                        case "STATUS":
                            responseText = "OK STATUS period=" + periodic.getPeriodMs() + ", threshold=" + periodic.getThreshold();
                            break;

                        default:
                            responseText = "ERROR Comandă necunoscută: " + action;
                            break;
                    }
                } catch (Exception e) {
                    // O comandă invalidă sau eroare de parsare nu oprește taskul
                    responseText = "ERROR " + e.getMessage();
                }

                // Publicăm confirmarea sau eroarea pe topicul de status
                String statusTopic = Config.statusTopic(deviceId);
                String payload = deviceId + ";" + timestamp + ";" + responseText;
                mqtt.publish(statusTopic, payload);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}