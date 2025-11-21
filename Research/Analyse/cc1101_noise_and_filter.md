# Analyse: Umgang mit Funkstörungen und ungültigen Paketen beim CC1101

Dieses Dokument beschreibt, wie ein wMBus‑Gateway mit dem Texas‑Instruments‑Transceiver **CC1101** so konfiguriert werden kann, dass Funkstörungen und zufällige Bitfolgen möglichst zuverlässig erkannt und verworfen werden. Die hier aufgeführten Maßnahmen beruhen auf Angaben im offiziellen **CC1101‑Datenblatt** und Erfahrungsberichten aus dem TI‑Support‑Forum.

## 1 Preamble Quality Threshold (PQT)

Der CC1101 überwacht beim Empfang den Wechsel der Bits im Vorlauf des Synchronworts. Ein interner Zähler erhöht sich, wenn 0‑ und 1‑Bit abwechseln, und sinkt stark ab, wenn sich Bits wiederholen. Erst wenn der Zähler einen programmierbaren **Preamble‑Quality Threshold (PQT)** erreicht, wird die Synchronwort‑Suche freigegeben. Der Schwellwert wird im Register `PKTCTRL1` über die Bits 7:5 (`PQT`) eingestellt. Der Wert **0** deaktiviert die Prüfung, höhere Werte erhöhen die Anforderung an den Preamble[^cc1101_datasheet_pqt].  
Die Software kann über das Status‑Bit `PKTSTATUS.PQT_REACHED` oder über einen auf einen GDO‑Pin geleiteten Interrupt erkennen, wann der Schwellwert überschritten ist[^cc1101_datasheet_pqt]. Im wMBus‑T‑Modus empfiehlt sich ein `PQT` zwischen 1 und 3, um kurzzeitige Rauschsequenzen auszufiltern.

## 2 Sync‑Qualifier und Carrier‑Sense

Das Register `MDMCFG2` definiert den **Sync‑Qualifier‑Modus**. Neben den einfachen Modi „15/16 Bit“ oder „16/16 Bit“ (Synchwort vollständig erkannt) kann die Synchronwortsuche mit dem **Carrier‑Sense‑Signal (CS)** verknüpft werden. CS wird aktiviert, wenn der gemessene RSSI über einen programmierbaren Schwellwert steigt. Die Kombination „16/16 + CS“ (Wert 6) akzeptiert ein Synchronwort erst, wenn alle 16 Bit übereinstimmen **und** CS aktiv ist. Dadurch werden sporadische Rauschschnipsel verworfen[^cc1101_datasheet_syncqual].  
Im TI‑Support‑Forum wird empfohlen, CS auf einen GDO‑Pin auszugeben (z. B. `IOCFG1=0x0E`) und die Software erst dann mit der Verarbeitung zu beginnen, wenn CS high ist[^ti_forum_cs].

## 3 Kanalfilter‑Bandbreite

Der CC1101 besitzt einen programmierbaren digitalen Kanalfilter, dessen Bandbreite in `MDMCFG4` über die Felder `CHANBW_E` und `CHANBW_M` eingestellt wird. Das Datenblatt enthält eine Tabelle mit möglichen Bandbreiten; typische Werte sind 406 kHz (E=1, M=0), 203 kHz (E=1, M=1) oder 135 kHz (E=2, M=2)[^cc1101_datasheet_bandwidth].  
Für den wMBus‑T‑Modus mit 100 kBaud (Frequenzhub ±50 kHz) reicht eine Bandbreite von 200–250 kHz. In stark verrauschten Umgebungen kann die Bandbreite verkleinert werden (z. B. auf 162 kHz), sofern die Frequenz‑Offset‑Kompensation entsprechend angepasst wird[^cc1101_datasheet_bandwidth].

## 4 Frequenz‑Offset‑Kompensation

Die automatische **Frequency Offset Compensation (AFC)** verhindert, dass sich Frequenzabweichungen zwischen Sender und Empfänger auf die Symbolerkennung auswirken. Im Register `FOCCFG` kann festgelegt werden, ab welchem Zeitpunkt die AFC aktiv wird. Das Bit `FOC_BS_CS_GATE` friert die Korrektur so lange ein, bis Carrier‑Sense aktiv ist; dadurch reagiert der Algorithmus nicht auf Rauschen[^cc1101_datasheet_foccfg]. Standardwerte für die Verstärkungen (`FOC_PRE_K` und `FOC_POST_K`) sollten im wMBus‑Betrieb beibehalten werden.

## 5 CRC‑Auto‑Flush und Statusauswertung

Mit aktiviertem `CRC_AUTOFLUSH` (in `PKTCTRL1` Bit 3) leert der CC1101 das RX‑FIFO automatisch, wenn die CRC‑Prüfung eines Pakets fehlschlägt. Diese Funktion verhindert, dass defekte Frames im FIFO verbleiben[^cc1101_datasheet_crc]. Wird zusätzlich `APPEND_STATUS=1` gesetzt, hängen RSSI und LQI am Ende jedes Pakets. Pakete mit niedriger LQI (z. B. unter 60) können softwareseitig verworfen werden.

## 6 Software‑Filter

Neben den Hardware‑Filtern sollten weitere Prüfungen im Mikrocontroller implementiert werden:

* **3‑out‑of‑6‑Decodierung:** Ungültige 6‑Bit‑Symbole führen zur Verwerfung des Pakets.
* **L‑Feld‑Konsistenz:** Das L‑Feld wird zweimal codiert; stimmen beide Werte nicht überein, ist der Frame ungültig.
* **CRC‑Prüfung:** Nach der Decodierung wird eine 16‑Bit‑CRC berechnet und mit dem im Paket enthaltenen CRC verglichen.
* **Access‑Number‑Debounce:** Pakete mit derselben Access‑Number wie das zuvor verarbeitete können ignoriert werden.
* **Minimale RSSI/LQI:** Pakete mit geringer Signalstärke oder schlechtem LQI werden verworfen.

Durch die Kombination aus **PQT**, **Sync‑Qualifier + CS**, **angepasster Kanalbandbreite**, **AFC‑Gate**, **CRC‑Auto‑Flush** und den genannten softwareseitigen Prüfungen lässt sich die Robustheit des Gateways in störbehafteten Umgebungen deutlich erhöhen.

## 7 AGC‑Parameter und Carrier‑Sense‑Schwelle

Neben den oben genannten Filtern hat auch die **automatische Verstärkungsregelung (AGC)** Einfluss auf die Empfindlichkeit und Störfestigkeit des CC1101. Das AGC‑Modul passt die Verstärkung der analogen LNAs und des digitalen VGAs an, um den Demodulator im optimalen Bereich zu halten. Im TI‑Support‑Forum wird empfohlen, die folgenden Parameter anzupassen, um eine ruhige Verstärkungsregelung zu erreichen und zufällige AGC‑Schaltvorgänge zu vermeiden:

* **Filterlänge und Wartezeit:** Das Feld `AGCCTRL0.FILTER_LENGTH` legt fest, wie viele RSSI‑Samples gemittelt werden, bevor eine Verstärkungsänderung erfolgt. Ein größerer Wert verlängert die Zeitkonstante und verhindert häufige Verstärkungswechsel bei Rauschen. Das Feld `AGCCTRL0.WAIT_TIME` definiert eine Pause nach jeder Verstärkungsanpassung, damit sich die Änderung im Signalpfad stabilisieren kann. Wird diese Wartezeit zu kurz gewählt, kann es zu Überschwingern kommen, die die Demodulation stören[^ti_forum_agc].

* **Zielpegel und CS‑Schwelle:** Der Zielpegel des AGC wird durch `AGCCTRL2.MAGN_TARGET` bestimmt. Ein höherer Wert verschiebt die interne Schaltschwelle nach oben, wodurch kleine Störsignale ignoriert werden. Über das Feld `AGCCTRL1.CARRIER_SENSE_ABS_THR` kann die absolute Carrier‑Sense‑Schwelle um ±7 dB relativ zum Zielpegel verschoben werden. Tabelle 7 in der TI‑Design‑Note DN505 zeigt die resultierenden Schwellen und wie sie sich aus MAGN_TARGET und CARRIER_SENSE_ABS_THR zusammensetzen[^dn505_threshold]. Wird der Zielpegel beispielsweise von 3 auf 7 erhöht und `CARRIER_SENSE_ABS_THR` von 0 auf 4 gesetzt, steigt der CS‑Schwellwert um 13 dB.

* **AGC einfrieren:** In Spezialanwendungen kann die AGC über `AGCCTRL0.AGC_FREEZE` deaktiviert und die Verstärkung manuell über das Register `AGCTEST` gesetzt werden. Dies setzt allerdings voraus, dass der Eingangssignalpegel bekannt ist und sich nicht ändert. Für wMBus‑Empfänger ist diese Option normalerweise nicht erforderlich.

Die Anpassung dieser Parameter ermöglicht eine feinere Abstimmung der Empfängerempfindlichkeit und hilft, unerwünschte Rauschsymbole zu unterdrücken. Sie sollte immer zusammen mit den PQT‑ und CS‑Filtern sowie dem schmaleren Kanalfilter betrachtet werden, um die gewünschte Robustheit zu erreichen.

[^cc1101_datasheet_pqt]: Texas Instruments. *CC1101 Low‑Power RF Transceiver Datasheet*. Abschnitt „Preamble Quality Threshold“ (Kapitel 17.2). Hier wird erklärt, wie der Pre­amble‑Qualitätszähler arbeitet und wie das Feld `PKTCTRL1.PQT` den benötigten Schwellwert festlegt.
[^cc1101_datasheet_syncqual]: Texas Instruments. *CC1101 Low‑Power RF Transceiver Datasheet*. Tabelle der Sync‑Qualifier‑Modi. Der Modus „16/16 + Carrier Sense“ (Wert 6) setzt voraus, dass das Synchronwort vollständig erkannt wurde und der RSSI über dem CS‑Schwellwert liegt.
[^ti_forum_cs]: Texas Instruments E2E Support Forum. *How to filter noise on CC1101 asynchronous mode?* Der Beitrag empfiehlt, Carrier‑Sense auf einen GDO‑Pin auszugeben (z. B. `IOCFG1=0x0E`) und erst zu dekodieren, wenn CS aktiv ist.
[^cc1101_datasheet_bandwidth]: Texas Instruments. *CC1101 Low‑Power RF Transceiver Datasheet*. Tabelle „Receiver Channel Filter Bandwidth“ (Kapitel 14). Sie zeigt die möglichen Kombinationen von `CHANBW_E` und `CHANBW_M` und die dazugehörigen Bandbreiten.
[^cc1101_datasheet_foccfg]: Texas Instruments. *CC1101 Low‑Power RF Transceiver Datasheet*. Abschnitt zur Frequency Offset Compensation (`FOCCFG`), in dem das Gate‑Bit `FOC_BS_CS_GATE` beschrieben wird.
[^cc1101_datasheet_crc]: Texas Instruments. *CC1101 Low‑Power RF Transceiver Datasheet*. Beschreibung des Registers `PKTCTRL1`: das Bit `CRC_AUTOFLUSH` leert das FIFO bei einem CRC‑Fehler und `APPEND_STATUS` fügt RSSI/LQI an.
[^ti_forum_agc]: Texas Instruments E2E Support Forum. *CC1101 – Don’t know the correct registers configuration.* In dieser Diskussion wird empfohlen, die AGC‑Parameter `AGCCTRL0.FILTER_LENGTH` und `AGCCTRL0.WAIT_TIME` zu erhöhen, um häufige AGC‑Schaltvorgänge zu vermeiden, und es wird erläutert, wie `AGCCTRL2.MAGN_TARGET` und `AGCCTRL1.CARRIER_SENSE_ABS_THR` den Carrier‑Sense‑Schwellwert beeinflussen. URL: https://e2e.ti.com/support/wireless-connectivity/sub-1-ghz-group/sub-1-ghz/f/sub-1-ghz-forum/382066/cc1101---don-t-know-the-correct-registers-configuration

[^dn505_threshold]: Texas Instruments. *Design Note DN505 – RSSI Interpretation and Timing*. Tabelle 7 zeigt, wie MAGN_TARGET und CARRIER_SENSE_ABS_THR kombiniert werden, um die absolute Carrier‑Sense‑Schwelle zu bestimmen. PDF: https://www.ti.com/lit/pdf/swra114