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
