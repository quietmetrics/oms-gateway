# Analyse der Radiocrafts‑Applikationsnote „AN043 – Wireless M‑Bus Security“

Die Applikationsnote AN043 von Radiocrafts (2020) erläutert die Sicherheitsmechanismen des Wireless‑M‑Bus‑Standards, insbesondere die verschiedenen Verschlüsselungsmodi und Sicherheitsprofile. Da viele Zählerdaten verschlüsselt übertragen werden, ist ein Verständnis dieser Modi für ein Gateway wichtig, das entschlüsseln oder zumindest die korrekten Schlüssel verwalten muss.

## Grundprinzipien der Sicherheit

Radiocrafts weist darauf hin, dass drahtlose Zählerdaten Schutzbedarf hinsichtlich **Vertraulichkeit**, **Integrität**, **Authentizität** und, bedingt, **Nicht‑Abstreitbarkeit** haben. Dafür kommen symmetrische Verschlüsselungsverfahren zum Einsatz, weil sie in batteriebetriebenen Geräten effizient sind. Eine typische Angriffsfläche ist das Key‑Management[^radiocrafts_security].

## Sicherheitsmodi und Profile

Das Dokument benennt mehrere Sicherheitsmodi des WMBus Link‑ und Transport‑Layers[^radiocrafts_security]:

| Modus | Verfahren | Merkmale |
|------|-----------|-----------|
| **Mode 5** | AES‑128‑CBC (nur Verschlüsselung) | Statische Schlüssel; Initialisierungsvektor (IV) wird aus Frame‑Info und Datenheader abgeleitet. Wird von vielen Zählern genutzt; einfache Implementierung[^radiocrafts_security]. |
| **Mode 7** | AES‑128‑CBC mit AES‑128‑CMAC | Zusätzlich zur Verschlüsselung wird ein Message Authentication Code (MAC) übertragen. Ephemerale Schlüssel werden mit einer Schlüsselableitung aus dem Nachrichtenzähler erzeugt[^radiocrafts_security]. |
| **Mode 8** | AES‑128‑CTR und AES‑128‑CMAC | Wird im Wize‑Protokoll verwendet. Zähler sind stets verschlüsselt (Counter‑Mode) und die MACs dienen zur Authentifizierung[^radiocrafts_security]. |
| **Mode 13** | TLS 1.2 (AES‑128‑CCM/GCM) | Für sehr hohe Sicherheitsanforderungen; benötigt asymmetrische Verschlüsselung und wird vor allem in Deutschland eingesetzt[^radiocrafts_security]. |

Die **Open Metering System (OMS)**‑Spezifikation greift diese Modi auf und definiert drei Sicherheitsprofile[^radiocrafts_security]:

* **Profil A**: Mode 5 (AES‑128‑CBC) ohne MAC; statischer Schlüssel.
* **Profil B**: Mode 7 (AES‑128‑CBC + CMAC) mit 8‑Byte‑MAC; Schlüssel werden pro Nachricht abgeleitet.
* **Profil C**: Mode 13 (TLS 1.2) mit HMAC/CMAC und elliptischer Kryptographie; sehr aufwendig, vor allem für Deutschland.

## Bedeutung für den Leitfaden

Viele wMBus‑Zähler verschlüsseln ihre Nutzdaten. Die Implementierung eines Gateways muss daher die entsprechenden Sicherheitsmodi verstehen. Für unser T‑Modus‑Gateway ist vor allem **Mode 5 (AES‑128‑CBC)** relevant, weil die meisten Zähler diesen Modus verwenden. Das bedeutet:

* **Schlüsselverwaltung:** Ein statischer 128‑Bit‑Schlüssel pro Zähler muss hinterlegt werden. Der Initialisierungsvektor wird aus Feldern des Telegramms abgeleitet.
* **Entschlüsselung der Nutzdaten:** Nach der 3‑out‑of‑6‑Decodierung und CRC‑Prüfung müssen die chiffrierten Nutzdaten mit dem AES‑128‑Algorithmus entschlüsselt werden. Die ersten zwei Bytes nach dem CI‑Feld dienen als Verifikation (Encryption Mode 5 fügt zwei Bytes zur Integritätsprüfung vor dem verschlüsselten Payload an).  
* **Erweiterte Modi (Profile B/C):** Für einige Hersteller könnten Mode 7 bzw. Mode 13 verwendet werden. Diese erfordern zusätzliche Berechnungen (MAC, Schlüsselableitung) und sind deutlich komplexer. In der Praxis wird Mode 7 inzwischen häufiger eingesetzt.

Die Note betont außerdem, dass das eigentliche Sicherheitsrisiko meist die Schlüsselverwaltung darstellt und nicht die kryptographische Stärke. Dies wird bei der Gestaltung des Gateways – etwa durch sichere Speicherung der Schlüssel und der Unterstützung von Schlüsselupdates – berücksichtigt.

[^radiocrafts_security]: Radiocrafts AS. *AN043 – Wireless M‑Bus Security* (2020). Dieses Dokument beschreibt die Anforderungen an Vertraulichkeit, Integrität und Authentizität bei Wireless‑M‑Bus‑Telegrammen und erläutert die Sicherheitsmodi 5, 7, 8 und 13 sowie die OMS‑Sicherheitsprofile A–C. Online abrufbar z. B. unter: https://radiocrafts.com/uploads/rc232-wmbus-security-application-note.pdf