package ssatr.service;

import java.util.Scanner;

/**
 * Consola operatorului: citește comenzi de la tastatură.
<<<<<<< HEAD
=======
 *
 *   show                               afișează dashboard-ul
 *   <deviceId> <COMANDA> [parametru]   trimite o comandă unui device, de exemplu
 *                                      device-1 SET_INTERVAL 1000
 *   help                               afișează comenzile
 *   exit                               oprește serviciul
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
 */
public class CommandConsole implements Runnable {

    private final MqttConnection mqtt;
    private final Dashboard dashboard;

    public CommandConsole(MqttConnection mqtt, Dashboard dashboard) {
        this.mqtt = mqtt;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        Scanner scanner = new Scanner(System.in);
        printHelp();

        while (scanner.hasNextLine()) {
            String line = scanner.nextLine().trim();

            if (line.isEmpty()) {
                continue;
            } else if (line.equals("exit")) {
                break;
            } else if (line.equals("help")) {
                printHelp();
            } else if (line.equals("show")) {
                dashboard.print();
            } else {
                sendCommand(line);
            }
        }
    }

    private void sendCommand(String line) {
<<<<<<< HEAD
        // TODO 6: separăm deviceId de restul liniei ("device-1 SET_INTERVAL 1000" -> "device-1" și "SET_INTERVAL 1000")
        String[] parts = line.split("\\s+", 2);
        if (parts.length < 2) {
            System.out.println("Eroare: Format invalid. Utilizare: <deviceId> <COMANDĂ> [parametru]");
            return;
        }

        String deviceId = parts[0];
        String commandPayload = parts[1];

        // Publicăm comanda pe topicul dedicat dispozitivului
        String topic = Config.commandTopic(deviceId);
        mqtt.publish(topic, commandPayload);
        System.out.println("Trimis pe " + topic + " -> " + commandPayload);
=======
        // TODO 6: separați deviceId de restul liniei
        //   ("device-1 SET_INTERVAL 1000" -> "device-1" și "SET_INTERVAL 1000")
        //   și publicați comanda pe Config.commandTopic(deviceId).
        //   Afișați un mesaj de eroare dacă linia nu are formatul corect.
        System.out.println("(trimiterea comenzilor neimplementată): " + line);
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
    }

    private void printHelp() {
        System.out.println("Comenzi: show | <deviceId> SET_INTERVAL <ms> | <deviceId> SET_THRESHOLD <valoare>");
        System.out.println("         <deviceId> RESET | <deviceId> STATUS | help | exit");
    }
<<<<<<< HEAD
}
=======
}
>>>>>>> 58ac9623f14d8d09fe205def8554a934697ddac3
