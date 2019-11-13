# TonUINO_Fork-Affenbox
Own TonUINO Fork
Mein eigener Fork der TonUINO DEV 2.1
- Start Up Sound und ShutDown Sound eingefügt.
- Zu-/Abschalten des Lautsprechers über Pin 8
- Ergänzung einer Zellspannungsmessung über einen Spannungsteiler, inkl. Ausgabe des Ladezustands über eine RGB LED
      Update: Damit einzelne falsche Messwerte nicht direkt zum ausschalten führen, wurde ein Counter eingebaut, der erst nach 10     aufeinanderfolgende Messwerten     die nächste Schwelle übernimmt 
- Ergänzung Ausschalten über langen druck auf Pause Taste
- Zellspannungsmessung ist jetzt über ein defien zu/abwählbar
- Serielle Ausgaben können über ein #define zu/abgeschaltete werden (27% Speicherersparnis ohne Ausgaben)
- beschleunigter setup() durch anpassung der Reihenfolge und entferen des 2sec Delays nach der initialisierung des DFPlayers

Neues Feature: Puzzle Spiel

Ein simples Spiel für den TonUINO, dass zwei zueinander gehörende Karten erwartet.

Funktion:
1. Als erstes müsst Ihr die Modifikationskarte "Puzzlespiel" über das Admin Menü erstellen.

2. Danach müsst ihr zwei Puzzleteile über die Normale Kartenkonfiguration erstellen. 
Hierbei wählt ihr wie gewohnt den Ordner in dem eure MP3s für das Spiel liegen.
Wählt nun den Modus Puzzle, der letzte Menüpunkt.
Jetzt wählt ihr das MP3.
Zum Schluss müsst ihr dem Puzzleteil eine Nummer geben. Teile mit gleicher Nummer gehören zusammen. Die beiden Teile MÜSSEN aber mindestens unterschiedliche MP3s haben oder in verschieden Ordnern liegen!

Die Puzzleteile können im Normalen Betrieb wie ein Einzeltrack abgespielt werden.

3. Aktiviert das Spiel durch Auflegen der Modifikationskarte.

4. Jetzt seit Ihr im Spiel. 
Das Spiel akzeptiert nur zum Spiel gehörende Karten alle anderen Karten werden mit einem Signalton abgelehnt.
Ihr könnt nun ein beliebiges Puzzleteil auflegen, und das MP3 dazu wird abgespielt. Anschließend kann folgendes passieren:
- Ihr legt ein Puzzleteil mit der gleichen Nummer auf (ausgenommen das gerade aufgelgte) und bekommt ein bestätigendes Signal. Teil gefunden! Jetzt kann man ein neues Teil auflegen und weiter spielen.
- Ihr legt ein Teil mit einer anderen Nummer auf. Ein Fehlerton ertönt und Ihr müsst weiter nach dem richtigen Teil suchen.
- Ihr haltet den Up & Down Button gleichzeitig gedrückt und last ihn nach ca 1s los. Das aktuell gespeicherte Teil wird gelöscht und man kann eine neue Runde starten.
- Ihr könnt den Pausetaster drücken und hört die letzte Ausgabe erneut oder stoppt eine laufende Ausgabe

5. Das Spiel wird durch erneutes Auflegen der Modifikationskarte beendet.
