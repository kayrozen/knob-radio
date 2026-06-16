# Plan de prototype — Board dual-chip (S3 + U4WDH) via PCM sur UART (COBS)

### Validation logicielle de l'architecture « pont PCM-UART » pour Préset

**Matériel** : module knob Guition JC3636K518 — ESP32-S3 (R8, 8MB PSRAM) +
ESP32-U4WDH (dual-core LX6, 4MB flash, sans PSRAM), reliés par UART, écran rond,
encodeurs rotatifs, haptique, DAC PCM5100. (même que waveshare knob)

**Statut du matériel** : validé par analyse des schématiques. Ce plan ne couvre
**que les tests logiciels**.

**Objectif** : valider en firmware que ce board peut faire de la radio internet
vers une voiture en Bluetooth, selon l'architecture :

```
ESP32-S3 (WiFi + HLS + tous codecs → PCM) → UART/COBS → ESP32-U4WDH (PCM → A2DP source) → voiture
```

---

## 1. La question à laquelle ce prototype répond

> **Ce board peut-il transporter un flux PCM (44.1kHz/16/stéréo, ~176 KB/s) du S3
> vers l'U4WDH par leur UART encapsulé en COBS, et le faire émettre en A2DP source
> vers une voiture, de façon stable pendant 1h+, sans coupures audibles et sans
> dérive incontrôlée?**

Si **oui** → ce board devient la cible matérielle de Préset.
Si **non** → retour au WROVER-E mono-chip (plan principal).

---

## 2. Faits matériels validés (acquis, non à re-tester)

| Fait | Détail confirmé |
|---|---|
| **Lien UART — chemin PCM** | S3 **GPIO38 (TX)** → U4WDH **GPIO18 (RX)** |
| **Lien UART — canal retour** | U4WDH **GPIO23 (TX)** → S3 **GPIO48 (RX)** |
| **Backpressure possible** | Le canal retour est câblé → l'U4WDH peut signaler « ralentis/accélère » au S3 |
| **Broches UART** | GPIO ordinaires, routables UART via matrice GPIO, aptes au haut débit |
| **Flashage** | **Deux ports USB-C, un par puce** : le côté branché détermine quelle puce est programmée |
| **I2S inter-puces** | Impossible (les deux I2S vont au commutateur CH445P → DAC) → l'UART est l'unique chemin de données |
| **Charge des puces** | U4WDH quasi vide (idéal pour pont A2DP) ; S3 chargé mais a tout le nécessaire |
| **Alimentation** | 5V USB → DCDC 3.3V + LDO propre audio ; alimentable par USB voiture |

---

## 3. L'architecture testée

Toute la complexité (WiFi, codecs, HLS, resampling, UI) sur le S3. L'U4WDH est un
pont bête et fiable : PCM par UART → A2DP. Aucun décodeur audio sur la puce sans
PSRAM.

---

## 4. Les inconnues logicielles à casser

| Risque logiciel | Pourquoi incertain | Phase |
|---|---|---|
| **A2DP source tient dans la SRAM de l'U4WDH** | Pas de PSRAM ; combien reste pour le tampon de gigue? | A |
| **UART DMA + COBS à 3 Mbps soutenu sans perte** | Pas de flow control matériel | B |
| **Resynchro COBS instantanée après corruption** | Le délimiteur 0x00 doit toujours réaligner | B |
| **Tampon de gigue bien dimensionné** | Trop petit = underrun ; trop gros = déborde la SRAM | C |
| **Maîtrise de la dérive d'horloge via backpressure** | Trois horloges sur longue durée | D |
| **Latence totale acceptable** | Décodage + COBS + UART + gigue + SBC + tampon voiture | C |
| **L'UI (LVGL) ne starve pas l'audio du S3** | Le S3 fait audio ET interface | E |
| **Changement de station sans casser le lien A2DP** | Reconfigurer le pipeline S3 pendant que l'U4WDH garde la connexion | E |

---

## 5. Environnement logiciel

| Puce | Framework | Rôle firmware |
|---|---|---|
| **ESP32-S3** | ESP-ADF (release stable v2.x) | WiFi + HLS + codecs → PCM → COBS → UART ; UI LVGL ; backpressure |
| **ESP32-U4WDH** | ESP-IDF nu (v5.x) + A2DP source | UART RX → COBS-décode → tampon → A2DP source ; backpressure |

- Deux projets séparés. Flashage : USB-C côté S3 / côté U4WDH.
- A2DP source : exemple `a2dp_source` d'ESP-IDF ou `pschatzmann/ESP32-A2DP`.
- Côté S3 : réutilise le pipeline du prototype ESP-ADF ; seule la sortie change
  (UART/COBS au lieu d'A2DP).
- **Outil recommandé** : analyseur logique pour observer l'UART/COBS pendant le
  débogage (Phases B-D).

---

## 6. Le protocole UART : COBS

C'est le cœur du code neuf. COBS (Consistent Overhead Byte Stuffing) résout le
cadrage de trame sur un flux d'octets rapide, sans flow control matériel.

### 6.1 Pourquoi COBS plutôt qu'un SYNC magique

Un octet/mot de synchro (ex. `0xA55A`) peut apparaître **par hasard** dans des
données PCM et provoquer une fausse synchro. COBS encode la trame de sorte que
l'octet `0x00` **ne peut jamais apparaître** dans le corps ; on utilise alors
`0x00` comme délimiteur de fin de trame, **garanti unique**. Resynchro toujours
instantanée et non ambiguë — exactement ce qu'il faut à 3 Mbps sans contrôle de
flux.

**Surcoût** : borné à ~1 octet par 254 octets de données (≈ 0,4 % pour une trame
de 512 octets) + 1 octet délimiteur. Négligeable. *(Mesuré ici à ~1,6 % pour la
trame complète seq/length/payload/crc8 — voir la sortie des tests host.)*

### 6.2 La trame logique (en mémoire, AVANT COBS)

```c
typedef struct {
    uint8_t  seq;            // numéro de séquence (détection de trame perdue)
    uint16_t length;         // taille du PCM (utile si trames de taille variable)
    uint8_t  payload[512];   // PCM 16-bit stéréo entrelacé
    uint8_t  crc8;           // intégrité du contenu
} audio_frame_t;
```

On ne transmet **jamais** cette struct telle quelle. Elle est sérialisée, puis
COBS-encodée, puis suivie d'un `0x00` délimiteur.

### 6.3 Le format sur le fil (APRÈS COBS)

```
[ ... octets COBS-encodés de (seq, length, payload, crc8), aucun 0x00 ... ][ 0x00 ]
```

**Émetteur (S3)** : sérialiser → COBS-encoder → ajouter `0x00` → UART TX DMA.
**Récepteur (U4WDH)** : lire jusqu'à `0x00` → COBS-décoder → vérifier crc8 +
continuité de seq → pousser le PCM dans le tampon de gigue.

> Implémentation : voir `components/pcm_link/` (`cobs.c`, `pcm_frame.c`,
> `pcm_link_rx.c`). Tests de correction et de resynchro : `test/host/`.

### 6.6 Backpressure / horloges

Le S3 décode à ~temps réel ; l'U4WDH consomme au rythme A2DP. Horloges nominales
44100 Hz mais distinctes. Stratégie v1 :

- L'U4WDH vise un tampon de gigue (ex. 200-250 ms).
- Tampon trop plein → octet « ralentis » sur le canal retour (GPIO23 → GPIO48).
- Tampon trop vide → octet « accélère », ou insertion de silence.
- Raffiner en Phase D selon la dérive mesurée.

> Implémentation : `backpressure.c` (U4WDH, TX) et `backpressure_rx.c` (S3, RX).

---

## 7. Phases logicielles

### Phase A — Compilation + flash (acquis ici via CI)
- Projet ESP-IDF minimal sur le S3 → log « hello S3 ».
- Projet ESP-IDF minimal sur l'U4WDH → log « hello U4WDH ».
- **Critère** : ✅ Chaque puce se flashe et se monitore via son port USB-C.
  *(CI compile les deux cibles ; les bannières « hello » sont émises au boot.)*

### Phases B-D — Lien, gigue, dérive (implémentées ici, à mesurer sur matériel)
- B : UART DMA + COBS à 3 Mbps soutenu, intégrité, resynchro.
- C : tampon de gigue dimensionné, latence.
- D : maîtrise de la dérive via backpressure sur 2h.

### Phase E — Vrai pipeline réseau + UI (point d'intégration, non bâti ici)
- S3 : pipeline ESP-ADF complet (WiFi → http/hls_stream → décodage → PCM → COBS → UART).
- Tester MP3 Icecast, AAC Icecast, HLS.
- Écran rond (LVGL) + encodeur EC1 (changer de station).
- Changement de station sans casser le lien A2DP de l'U4WDH.
- **Critères** : radio réelle de bout en bout ; UI active sans coupure audio ;
  changement de station < 2s, lien A2DP survit.

### Phase F — Conditions réelles, vraie voiture
- Board alimenté par USB dans une voiture ; pairer avec le système BT réel.
- Conduire 30+ min, flux HLS et Icecast ; perte réseau + reprise ; AVRCP.

---

## 8. Livrable du prototype

Document court (3-4 pages) : verdict go/no-go + confiance ; mesures (intégrité
UART/COBS, surcoût COBS réel, resynchro, tampon de gigue, latence, dérive sur 2h,
RAM libre U4WDH, coupures/heure) ; protocole retenu ; points de friction ;
recommandation (board cible / repli WROVER-E) ; estimation révisée.

---

## 9. Ce que le prototype NE fait PAS

Captive portal / provisioning, télémétrie, OTA ; playlist complète ; haptique,
micro PDM, SD ; branding ; le DAC analogique PCM5100 (sortie Bluetooth) ;
vérification matérielle (déjà validée par les schématiques).

---

## 11. Relation avec le prototype ESP-ADF

Ce plan **absorbe** le prototype ESP-ADF mono-WROVER : le côté S3 **EST** le
pipeline ESP-ADF, seule la sortie change (COBS/UART au lieu d'A2DP). Réussir ce
prototype valide ESP-ADF **et** le board dual-chip. Si le board échoue mais que le
pipeline S3 marchait, on récupère la validation ESP-ADF pour le repli WROVER-E.

---

## 13. Questions ouvertes

- **Release ESP-ADF à figer** (dernière stable v2.x) : prévoir un docker pour la
  compilation via GitHub.
- **PCM ou ADPCM** dès le départ ? Recommandation : **PCM** (implémenté).
- **Taille de trame COBS optimale** : départ **512 oct PCM (~2,9 ms)** ; ajuster
  selon latence/overhead en Phase B.
- **Driver LVGL** : modèle `JC3636W518V2`.
- **AVRCP** côté U4WDH : à ajouter.
