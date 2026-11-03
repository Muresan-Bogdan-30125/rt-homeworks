package ssatr.service;

<<<<<<< HEAD
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Ultima stare cunoscută a fiecărui device: ultima temperatură, momentul
 * ultimului mesaj, numărul de alarme etc.
 * Accesul este sincronizat prin structuri thread-safe.
 */
public class Dashboard {

    // Clasa internă care reține starea pentru fiecare dispozitiv
    public static class DeviceState {
        String deviceId;
        double lastTemperature;
        long lastMessageTimestamp;
        int alarmCount;
        String lastStatusText;

        public DeviceState(String deviceId) {
            this.deviceId = deviceId;
            this.lastTemperature = 0.0;
            this.lastMessageTimestamp = System.currentTimeMillis();
            this.alarmCount = 0;
            this.lastStatusText = "N/A";
        }
    }

    // Map thread-safe pentru starea device-urilor (cheia este deviceId)
    private final Map<String, DeviceState> devices = new ConcurrentHashMap<>();

    /** Actualizează starea pe baza tipului de mesaj și a conținutului primit. */
    public synchronized void update(String kind, String message) {
        try {
            String[] parts = message.split(";", 4);
            if (parts.length < 2) return;

            String deviceId = parts[0];
            long timestamp = Long.parseLong(parts[1]);

            DeviceState state = devices.computeIfAbsent(deviceId, DeviceState::new);
            state.lastMessageTimestamp = timestamp;

            if (kind.equals("telemetry") && parts.length >= 3) {
                state.lastTemperature = Double.parseDouble(parts[2]);
            } else if (kind.equals("alarm")) {
                if (parts.length >= 3) {
                    state.lastTemperature = Double.parseDouble(parts[2]);
                }
                state.alarmCount++;
            } else if (kind.equals("status") && parts.length >= 3) {
                state.lastStatusText = parts[2];
            }
        } catch (Exception e) {
            System.out.println("Eroare la parsarea mesajului în Dashboard: " + e.getMessage());
        }
    }

    /** Afișează un tabel cu starea tuturor device-urilor. */
    public synchronized void print() {
        if (devices.isEmpty()) {
            System.out.println("Nu există dispozitive înregistrate momentan.");
            return;
        }

        long now = System.currentTimeMillis();
        System.out.println("\n--- DASHBOARD DISPOZITIVE IoT ---");
        System.out.printf("%-12s | %-10s | %-18s | %-8s | %-15s%n", "DEVICE", "TEMP (°C)", "ULTIMUL MESAJ", "ALARME", "STATUS");
        System.out.println("----------------------------------------------------------------------------------");

        for (DeviceState state : devices.values()) {
            long ageSec = (now - state.lastMessageTimestamp) / 1000;
            String ageStr = "acum " + ageSec + " s";
            System.out.printf("%-12s | %-10.2f | %-18s | %-8d | %-15s%n",
                    state.deviceId,
                    state.lastTemperature,
                    ageStr,
                    state.alarmCount,
                    state.lastStatusText
            );
        }
        System.out.println("----------------------------------------------------------------------------------\n");
    }
}
=======
/**
 * Ultima stare cunoscută a fiecărui device: ultima temperatură, momentul
 * ultimului mesaj, numărul de alarme etc.
 *
 * Este actualizată de thread-urile MessageProcessor și citită de CommandConsole,
 * deci accesul trebuie sincronizat.
 */
public class Dashboard {

    // TODO 5: alegeți structura de date pentru starea device-urilor
    //   (de exemplu un Map<String, ...> cu cheia deviceId)
    //   și adăugați metodele de actualizare apelate din MessageProcessor.

    /** Afișează un tabel cu starea tuturor device-urilor. */
    public void print() {
        // TODO 5: afișați câte un rând pentru fiecare device, de exemplu:
        //   DEVICE     TEMP    ULTIMUL MESAJ   ALARME
        //   device-1   23.41   acum 1 s        2
        System.out.println("(dashboard neimplementat)");
    }
}
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
