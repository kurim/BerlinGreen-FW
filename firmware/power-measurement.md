# GreenBox8C4B14C0: Messreihe Helligkeit zu Leistung, alle Farbtemperaturen

**Messbedingungen**
- Messung: Flex 2 (`sensor.wohnzimmer_flex_2`), Netzseite (230 V), Leistung in W
- Reihenfolge beim Messen: erst Farbtemperatur setzen, dann Helligkeit
- Nach jedem Punkt geprüft, ob die Farbtemperatur noch stimmt
- Helligkeitsskala ist farbtemperaturabhängig, das Maximum steht jeweils in der Spalte „Max“

**Übersicht**

| Farbtemperatur | Max (%) | Status |
|---|---:|---|
| 3000 K | 33 | vollständig, 0–33 % |
| 3700 K | 66 | vollständig, 0–66 % |
| 4500 K | 100 | vollständig, 0–100 % |
| 5300 K | 66 | ohne 0 %, 7–66 % gültig |
| 6000 K | 33 | 0–33 % gemessen, wahrscheinlich gültig, nicht vollständig |

**Messwerte je Farbtemperatur (Helligkeit % → Leistung W)**

*3000 K (Max 33)*

| % | 0 | 3 | 7 | 10 | 13 | 17 | 20 | 23 | 27 | 30 | 33 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| W | 1,2 | 1,7 | 2,5 | 3,4 | 3,9 | 4,8 | 5,4 | 6,1 | 7,0 | 7,8 | 8,6 |

*3700 K (Max 66)*

| % | 0 | 7 | 13 | 20 | 26 | 33 | 40 | 46 | 53 | 59 | 66 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| W | 0,6 | 2,8 | 4,1 | 5,6 | 7,0 | 8,2 | 9,9 | 11,1 | 12,6 | 14,0 | 15,7 |

*4500 K (Max 100)*

| % | 0 | 5 | 10 | 15 | 20 | 25 | 30 | 35 | 40 | 45 | 50 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| W | 0,5 | 3,1 | 4,0 | 5,0 | 6,0 | 7,2 | 8,2 | 9,5 | 10,6 | 11,8 | 13,0 |

| % | 55 | 60 | 65 | 70 | 75 | 80 | 85 | 90 | 95 | 100 |
|---|---|---|---|---|---|---|---|---|---|---|
| W | 14,1 | 15,1 | 16,2 | 17,2 | 18,3 | 19,3 | 20,5 | 21,5 | 22,5 | 23,5 |

*5300 K (Max 66, 0 % nicht gültig gemessen)*

| % | 7 | 13 | 20 | 26 | 33 | 40 | 46 | 53 | 59 | 66 |
|---|---|---|---|---|---|---|---|---|---|---|
| W | 3,1 | 4,0 | 5,8 | 6,8 | 8,8 | 10,5 | 11,6 | 12,8 | 14,6 | 16,0 |

*6000 K (Max 33, wahrscheinlich gültig)*

| % | 0 | 7 | 13 | 20 | 26 | 33 |
|---|---|---|---|---|---|---|
| W | 0,4 | 2,3 | 3,8 | 5,3 | 6,6 | 8,3 |

**Offene Punkte**
- 5300 K bei 0 %: Die Farbtemperatur springt beim Setzen auf 0 % auf 6000 K. Der Wert ist nicht sauber messbar.
- 6000 K: Die Reihe ist nur bis 33 % erfasst, da das Maximum dort 33 ist. Die Zwischenpunkte fehlen (z. B. 10, 17, 23, 27, 30).
- Die Farbtemperatur springt beim Setzen der Helligkeit teils zurück. Bei 3000, 3700 und 4500 K blieb sie stabil, bei 5300 K nicht durchgehend.

**Firmware-Stützpunkte (Beispiel für 4500 K, ESPHome lambda)**

```cpp
const float x[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100};
const float y[] = {0.5, 3.1, 4.0, 5.0, 6.0, 7.2, 8.2, 9.5, 10.6, 11.8, 13.0, 14.1, 15.1, 16.2, 17.2, 18.3, 19.3, 20.5, 21.5, 22.5, 23.5};
```
