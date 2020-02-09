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

## Neues Feature: Puzzle Spiel

Ein simples Spiel für den TonUINO, dass zwei zueinander gehörende Karten erwartet.
Ihr müsst dafür auch ein paar MP3s in eurem mp3 Ordner auf der SD-Karte ergänzen/ersetzen. Diese könnt ihr hier herunter laden.
Löscht bitte zuerst die vorhandenen Dateien 0310 - 0318 sowie die Dateien 0970 - 0976. Diese mussten wegen den erweiterten Modifire Tags umbenannt werden.

### Funktion:
1. Als erstes müsst Ihr die Modifikationskarte "Puzzlespiel" über das Admin Menü erstellen.

2. Danach müsst ihr zwei Puzzleteile über die Normale Kartenkonfiguration erstellen. 
Hierbei wählt ihr wie gewohnt den Ordner in dem eure MP3s für das Spiel liegen.
Wählt nun den Modus Puzzle, der 10. Menüpunkt.
Jetzt wählt ihr das MP3.
Zum Schluss müsst ihr dem Puzzleteil eine Nummer geben. Teile mit gleicher Nummer gehören zusammen. Die beiden Teile MÜSSEN aber mindestens unterschiedliche MP3 Nummern haben oder in verschieden Ordnern liegen!

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

## Neues Feature: Quiz 

Ein weiteres simples Spiel auf Basis des Puzzles. Im Unterschied zum Puzzle wird hier ein Sound, aus einem im Modifire Tag hinterlegten Ordner, zufällig abgespielt. Das Ziel ist es das dazugehörige Puzzle- /Antwortteil aufzulegen.

Ihr könnt so eure schon erstellten Puzzleteile und die dazugehörigen MP3s nutzen oder neue MP3s mit Fragen erstellen, die als Antwort die Puzzleteile erwarten.

Zu Beachten ist, das die MP3 Nr der Frage mit der Nummer des Puzzleteils übereinstimmen muss. Falls das Puzzleteil direkt auf die Frage zeigt (gleiche Ordner & MP3 Nr.) wird das Antwortteil nicht aktzeptiert.

## Neues Feature: Button Smash

Besonders kleine Kinder verstehen die Funktionen des TonUINO noch nicht so ganz und neigen dazu einfach alles aus zu probieren. Wild auf Knöpfe drücken oder wahllos Karten auflegen scheint ihnen besondere Freude zu bereiten.
Um die Kinder nicht mit der Tastensperre zu frustrieren, weil nichts passiert, könnt ihr stattdessen das Button Smash Spiel verwenden.
Hier werden aus einem Ordner, der im Modifire Tag hinterlegt ist, beliebig MP3s abgespielt.

Um die Ohren der Eltern zu schonen, müsst iht im Modifire Tag auch eine feste Lautstärke hinterlegen. Im Spiel sind alle üblichen Funktionen deaktiviert. 
Außerdem wird jede MP3 zuende gespielt, deshalb sollten die MP3s nicht zu lang sein.

## Inhalt der Tags

Die RFID Tags sind wie folgt beschrieben:

### Puzzle- /Antwortteil
Card Cookie: 0x13 37 B3 47

Ordner: 0x01 bis 0x63 (1 bis 99)

Modus: 0x0A (Puzzle/Quiz)

Extra: 0x01 bis 0xFF (1 bis 255) (Nummer des Tracks)

Extra2: 0x01 bis 0xFF (1 bis 255) (Nummer des Puzzleteils)

### Modifiertag Puzzle
Card Cookie: 0x13 37 B3 47

Ordner: 0x00

Modus: 0x08 (Puzzlespiel)

Extra: 0x00

Extra2: 0x00

### Modifiertag Quiz
Card Cookie: 0x13 37 B3 47

Ordner: 0x00

Modus: 0x09 (Puzzlespiel)

Extra: 0x01 bis 0x63 (1 bis 99) (Nummer des Ordners mit den Fragen)

Extra2: 0x00

### Modifiertag Button Smash
Card Cookie: 0x13 37 B3 47

Ordner: 0x00

Modus: 0x09 (Puzzlespiel)

Extra: 0x01 bis 0x63 (1 bis 99) (Nummer des Ordners mit den Sounds)

Extra2: 0x01 bis 0xC0 (1 bis 30) (Lautsärke der Sounds)



# Spiel Idee

Die Idee hinter diesen Spielen ist, mit nur einem Datensatz an MP3s, ein große Funktionsvielfalt zu erhalten.

Ihr habt z.B. 4 Ordner

01 : Tiergeräusche

02 : Tiernamen deutsch

03 : Tiernamen englisch

04 : Fragen zu Tieren

In jedem dieser Ordner sind z.B. 100 MP3s, wobei immer die erste MP3 Beispielswiese "Hund" ist.
Sprich:

in Ordner 01 ist die MP3 001, bellen

in Ordner 02 ist die MP3 001, Hund ausgesprochen

in Ordner 03 ist die MP3 001, Dog ausgesprochen

in Ordner 04 die MP3 001, "Was ist des Menschen bester Freund?"

Jetzt legt Ihr zu den MP3 in den ersten 3 Ordnern je ein Puzzle-/Antwort Tag an.
Ordner 04_ hinterlegt Ihr im Quiz Modifier Tag und Ordner 01 im Button Smash Tag.

Die Puzzle-/Antwort Tags bedruckt Ihr dann entsprechend mit Bildern oder Wörtern.

Wenn ihr zusätzlich noch eine Auswahl an Puzzle-/Antwort Tags unbedruckt lasst, könnt ihr daraus ein akustisches Memory erzeugen.

# Quelle für MP3s
Eine Datenbank mit Piktogrammen und MP3 mit Passenden Wörtern in verschiednen Sprachen findet ihr hier
http://www.arasaac.org/descargas.php
