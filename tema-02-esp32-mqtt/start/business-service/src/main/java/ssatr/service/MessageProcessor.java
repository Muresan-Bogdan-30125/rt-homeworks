package ssatr.service;

import java.util.concurrent.BlockingQueue;

/**
 * Preia mesajele de un anumit tip ("telemetry", "alarm" sau "status") din coada
 * inbox, le salvează și actualizează dashboard-ul.
 */
public class MessageProcessor implements Runnable {

    private final String kind;
    private final BlockingQueue<String> inbox;
    private final CsvStorage storage;
    private final Dashboard dashboard;

    public MessageProcessor(String kind, BlockingQueue<String> inbox, CsvStorage storage, Dashboard dashboard) {
        this.kind = kind;
        this.inbox = inbox;
        this.storage = storage;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        try {
            while (!Thread.currentThread().isInterrupted()) {
                // 1. Preluăm mesajul din coadă (blocant)
                String message = inbox.take();

                try {
                    // 2. Salvăm mesajul în fișierul CSV corespunzător
                    storage.append(kind, message);

                    // 3. Actualizăm dashboard-ul
                    dashboard.update(kind, message);

                    // 4. Alarmele și confirmările (status) se afișează imediat în consolă
                    if (kind.equals("alarm") || kind.equals("status")) {
                        System.out.println("[" + kind.toUpperCase() + " PRIMIT] " + message);
                    }

                } catch (Exception e) {
                    // Un mesaj cu format invalid sau eroare de procesare nu trebuie să oprească thread-ul
                    System.out.println("Eroare la procesarea mesajului [" + kind + "]: " + message + " -> " + e.getMessage());
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}