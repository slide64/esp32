# Monstre Halloween — Waveshare ESP32-S3 1.85C-BOX

Firmware Arduino pour **ESP32-S3-Touch-LCD-1.85C**, écran rond ST77916
360×360, codec ES8311 et boîtier haut-parleur. Contrôle Home Assistant via
votre broker MQTT existant et découverte automatique. Ne pas utiliser les
brochages de la variante 1.85 sans « C ».

## Fonctions

- GIF animés citrouille (`pumpkin`), démon (`demon`), zombie (`zombie`).
- Yeux animés intégrés lorsque le GIF ou la microSD manque.
- Ambiance musicale en boucle, voix ou effets ponctuels sur le haut-parleur.
- Entités Home Assistant : monstre, son, volume, activation, faire peur,
  arrêter le son, lecture en cours et diagnostics microSD/audio.
- Retour d'état MQTT, disponibilité avec Last Will et reconnexion.

La voix interrompt la musique ; cette version ne mélange pas deux pistes.
Les voix sont des fichiers enregistrés, sans synthèse vocale ni microphone.
Le Bluetooth n'est pas nécessaire pour les commandes Home Assistant.

## 1. Préparer la compilation

Installer Python et PlatformIO Core **6.1.18**, ou utiliser l'extension PlatformIO
pour VS Code. Le projet fixe pioarduino **54.03.21-2** (Arduino ESP32 **3.2.1**),
LVGL **9.3.0** et PubSubClient **2.8**. Les sources des pilotes Waveshare sont
incluses ; voir `THIRD_PARTY.md`.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install platformio==6.1.18 Pillow==11.3.0 esp-idf-size==1.6.1
cp include/config.example.h include/config.local.h
```

Compléter `include/config.local.h` avec le Wi-Fi **2,4 GHz**, l'adresse du broker
MQTT et son compte dédié. Le fichier est ignoré par Git. Ne pas publier ses
identifiants. Donner à ce compte accès à `halloween/halloween185c/#`,
`homeassistant/#` pour la découverte et la lecture de `homeassistant/status`.
Un identifiant `DEVICE_ID` distinct est nécessaire par appareil.

MQTT utilise ici le port **1883**, sans TLS : prévu pour votre réseau local.
Ne pas exposer ce broker à Internet. Aucun accès à votre Home Assistant privé
n'est nécessaire dans l'environnement cloud pour compiler.

Dans le cloud, le venv existe déjà et le script suivant sélectionne les bons
chemins d'outils. En local, il utilise `pio` depuis votre PATH :

```sh
./scripts/pio.sh run -j 2
```

Sans `config.local.h`, le firmware compile et affiche l'animation autonome,
mais ne tente aucune connexion Wi-Fi/MQTT.

## 2. Préparer la microSD

Utiliser une carte **FAT32**. Les médias de démonstration prêts à copier sont
dans **`demo-media/`**, inclus dans le ZIP GitHub : copier ses dossiers
`monsters/` et `audio/` à la racine de la microSD.

Pour générer une autre copie dans `media/` :

```sh
python scripts/make_media.py
```

Dans le cloud : `/workspace/tools/halloween-venv/bin/python scripts/make_media.py`.
Copier **le contenu** de `media/` à la racine de la microSD :

```text
/monsters/pumpkin.gif
/monsters/demon.gif
/monsters/zombie.gif
/audio/ambiance.wav
/audio/pumpkin.wav
/audio/demon.wav
/audio/zombie.wav
```

Les trois GIF sont des dessins animés originaux de démonstration. Les WAV fournis
sont des ambiances et effets synthétiques, **pas des voix parlées**. Remplacer les
WAV des monstres par vos enregistrements de voix. Vous pouvez aussi remplacer
l'ambiance par votre musique. La copie générée dans `media/` est ignorée par Git et ses fichiers
peuvent être recréés. Le générateur conserve les fichiers déjà présents.

Format obligatoire : **WAV PCM signé 16 bits, mono, 24 000 Hz**. Conversion :

```sh
ffmpeg -i ma_voix.mp3 -ar 24000 -ac 1 -c:a pcm_s16le media/audio/demon.wav
```

Les GIF doivent mesurer **360×360** ; commencer avec 10–15 images/s et une
palette de 32–128 couleurs pour limiter les accès microSD. Exemple :

```sh
ffmpeg -i monstre.gif -filter_complex \
  '[0:v]fps=12,scale=360:360:force_original_aspect_ratio=decrease,pad=360:360:(ow-iw)/2:(oh-ih)/2:black,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse' \
  -loop 0 media/monsters/demon.gif
```

Insérer la carte avant la mise sous tension ; redémarrer après remplacement
ou ajout de fichiers. Le bouton « Faire peur » relance le GIF sélectionné et
joue `/audio/<monstre>.wav`. Un WAV absent ou incompatible est signalé sur le
port série ; la partie visuelle reste utilisable.

## 3. Flasher et vérifier sur la carte

Connecter la carte par USB avec un câble de données :

```sh
./scripts/pio.sh run --target upload
./scripts/pio.sh device monitor
```

Le port série est à 115200 bauds. En cas de problème de connexion, maintenir
BOOT pendant RESET puis réessayer. Utiliser `--upload-port` avec le port série
requis si plusieurs appareils sont branchés.

Vérifier successivement : animation de démarrage, `microSD=OK audio=OK` sur
le port série, connexion réseau, puis appareil « Monstre Halloween » dans
**Paramètres → Appareils et services → MQTT** dans Home Assistant. L'intégration
MQTT et sa découverte doivent être activées. Tester d'abord le volume à 20–30 %.
Le défaut est 30 %. L'interrupteur « Actif » coupe le rétroéclairage et le son ;
il ne met pas l'ESP32 en veille et ne coupe pas son Wi-Fi.

Les paramètres de scène et de volume reviennent aux valeurs par défaut après
redémarrage. Une automation HA peut les remettre à la connexion de l'appareil.

## 4. Automatisations Home Assistant

Adapter l'exemple dans `homeassistant/automations.yaml` à votre détecteur.
Les commandes MQTT ci-dessous évitent de dépendre des noms d'entités générés
par Home Assistant. Remplacer `halloween185c` si vous changez `DEVICE_ID`.

| Topic | Payload |
| --- | --- |
| `halloween/halloween185c/monster/set` | `pumpkin`, `demon`, `zombie` |
| `halloween/halloween185c/track/set` | `ambiance`, `voix`, `silence` |
| `halloween/halloween185c/volume/set` | entier de `0` à `100` |
| `halloween/halloween185c/power/set` | `ON`, `OFF` |
| `halloween/halloween185c/scare/set` | `PRESS` |
| `halloween/halloween185c/stop/set` | `PRESS` |

Envoyer les commandes **sans retain**, pour éviter de rejouer une scène lors
d'une reconnexion. Les états et la découverte sont retenus. L'état JSON arrive
sur `halloween/halloween185c/state`, la disponibilité sur
`halloween/halloween185c/availability`. Si vous renommez l'appareil, supprimer
les anciennes entités MQTT et leurs topics de découverte retenus.

## Matériel et validation

Broches reprises des exemples Waveshare 1.85C : I²C SDA=11/SCL=10 ;
QSPI CLK=40, D0=46, D1=45, D2=42, D3=41, CS=21 ; rétroéclairage=5 ;
reset écran via EXIO2 du TCA9554 ; SD CLK=14/CMD=17/D0=16 ;
audio MCLK=2/BCLK=48/LRCLK=38/DOUT=47 ; amplificateur=15.
Configuration mémoire visée : flash 16 Mo et PSRAM OPI 8 Mo.

Test du lecteur WAV, y compris fichiers tronqués et formats incompatibles :

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I include tests/test_wav.cpp -o /tmp/test_halloween_wav
/tmp/test_halloween_wav
```

La compilation cloud et les tests logiciels ne remplacent pas la validation
sur la carte : couleurs, orientation, fluidité simultanée GIF/audio, volume,
connexion à votre broker et déclenchement des automatisations restent à vérifier
avec votre matériel et votre réseau.

## Diagnostic audio sans moniteur série

### Test autonome du haut-parleur (sans carte SD ni réseau)

Le target PlatformIO **audio_selftest** isole le codec, l'I2S et l'amplificateur
de l'application Halloween. Il utilise le brochage du [schéma Waveshare V2](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/main/hardware/schematics/ESP32-S3-Touch-LCD-1.85C-V2--Schematic.pdf_.pdf)
et la séquence du [test audio officiel 03](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/Arduino/examples/03_audio_out_no_tf/03_audio_out_no_tf.ino).
Le schéma confirme SDA=11, SCL=10, MCLK=2, BCLK=48, LRCLK=38, données=47,
amplificateur=15 ; le codec est ES8311 à 0x18. Son alimentation A3V3 a son
régulateur activé matériellement, sans GPIO supplémentaire à activer.

Ce test remplace temporairement le firmware sur la carte. Il ne démarre ni
l'écran, ni le Wi-Fi, ni MQTT, ni la microSD. Les identifiants du projet ne sont
pas utilisés. Un haut-parleur doit être connecté au connecteur prévu.

Dans VS Code : **PlatformIO → PROJECT TASKS → audio_selftest → General → Upload**,
puis **Monitor** dans le même groupe. Ou, dans le terminal PlatformIO :

```sh
pio run -e audio_selftest -t upload --upload-port COM3
pio device monitor -e audio_selftest --port COM3
```

Dans le cloud, `./scripts/pio.sh run -e audio_selftest -j 2` vérifie uniquement
la compilation. L'exécution requiert la carte physique.

Un bip de 600 Hz, limité à environ 8 % de l'amplitude PCM maximale, doit durer
une demi-seconde toutes les deux secondes. Le moniteur répète toutes les trois
secondes `audio-selftest-1 | I2C ACK:... | audio=...` pour ne pas dépendre de
la capture des premières lignes au démarrage. En cas d'échec du codec,
l'amplificateur reste désactivé. La séquence officielle configure le codec
avant l'I2S ; si elle échoue, le test réessaie avec MCLK actif.

Un bip audible valide le codec, l'amplificateur et le haut-parleur dans ce
test isolé. Un échec d'initialisation n'établit pas à lui seul une panne
matérielle : conserver le code I2C et les adresses détectées pour le diagnostic.
Pour remettre l'animation et Home Assistant, lancer ensuite **Upload** sous
**halloween**, qui reste l'environnement par défaut.

Le fichier fourni `audio_player_test.c` définit les broches d'**ESP-BOX**, pas
celles de la Waveshare 1.85C ; sa fonction mute ne fait que journaliser et ne
configure pas l'ES8311. `audio_player.h` fournit l'interface d'un lecteur, pas
son pilote matériel. Ils ont été utilisés comme références, sans importer
leurs broches dans le firmware Waveshare.

### Diagnostic du firmware Halloween

La version `audio-diag-5` conserve les deux transactions ES8311 en échec
(adresse, registre, lecture/écriture et code I2C ou nombre d'octets reçus), puis
liste les adresses connues qui acquittent sur le bus : tactile 0x15, codec
0x18/0x19, expandeur 0x20 et horloge 0x51. Les sondes sont limitées à ces
adresses et n'écrivent aucun registre. Les mêmes informations restent visibles
dans `Diagnostic audio`, les attributs d'`Audio disponible` et le résumé série
toutes les dix secondes. Ce diagnostic ne corrige pas à lui seul un codec qui
ne répond pas et ne prouve pas une panne matérielle.

La version `audio-fix-4` démarre l'horloge I2S avant de configurer le codec,
désactive explicitement son mode muet et fixe son gain à 0 dB. Le volume reste
réglé par Home Assistant (30 % au démarrage) : l'ancien réglage du codec ajoutait
environ 13 dB d'atténuation. Après compilation et flash, sélectionner
« ambiance » ou « Faire peur » pour tester ; le démarrage reste silencieux.
La microSD doit contenir les WAV au format indiqué plus haut.
Le pilote essaie l'initialisation par des transactions de registre aux deux
adresses ES8311 documentées (0x18 et 0x19), sans abandonner sur le seul résultat
d'une sonde I2C vide. Ce changement suit l'initialisation directe des exemples
Waveshare ; il ne prouve pas que la sonde vide était la cause du refus I2C
sur une carte donnée. Si les transactions réelles échouent également, l'audio
reste signalé indisponible et l'amplificateur reste désactivé.
Les lectures I2C incomplètes et les erreurs d'écriture sont signalées comme
des échecs. Les buffers DMA et la tâche audio sont alloués avant le chargement
des GIF pour préserver la mémoire interne nécessaire au son.
L'entité existante **Audio disponible** expose désormais les attributs
`diagnostic` et `firmware`, sans dépendre du moniteur série ni du nom du capteur
de diagnostic séparé.

Le test logiciel d'initialisation, avec matériel simulé, se lance avec
`python3 tests/check_audio_init.py`. Il vérifie le cas d'une sonde vide refusée
avec initialisation par registre réussie, les deux adresses, la répétition
de l'initialisation et neuf chemins d'échec. `python3 tests/check_codec_i2c.py`
exerce le vrai pilote ES8311 avec des lectures/écritures I2C simulées. Ces tests
ne remplacent pas un essai du haut-parleur sur la carte.

Références utilisées pour le correctif :

- [Exemple Waveshare 03 : sortie audio sans microSD](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/Arduino/examples/03_audio_out_no_tf/03_audio_out_no_tf.ino)
- [Exemple Waveshare 05 : sortie audio depuis la microSD](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/Arduino/examples/05_audio_out_tf/05_audio_out_tf.ino)
- [Pilote I2S Espressif, activation de MCLK et initialisation](https://github.com/espressif/esp-idf/blob/v5.4/components/esp_driver_i2s/i2s_std.c)
- [Pilote ES8311 Espressif, échelle du gain et commande mute](https://github.com/espressif/esp-adf/blob/master/components/esp_codec_dev/device/es8311/es8311.c)

La version `audio-diag-1` ajoute l'entité **Diagnostic audio** à la section
Diagnostic de l'appareil MQTT. Si **Audio disponible** est désactivé, relever
le texte de cette entité : il distingue la détection I²C du codec ES8311,
sa configuration, l'initialisation I²S et l'allocation de la tâche audio.
Ce diagnostic ne prouve pas à lui seul que le haut-parleur émet du son.

Pour mettre à jour un projet Windows extrait d'un ZIP : conserver le projet
actuel et son `include/config.local.h`, télécharger le nouveau ZIP, puis
copier/coller les dossiers `src` et `include` de la nouvelle archive dans le
projet existant en remplaçant les fichiers proposés. Ne pas supprimer les
anciens dossiers : `config.local.h` n'est pas inclus dans l'archive et doit
rester en place. Ensuite lancer **Build**, puis **Upload** et relire l'entité
Diagnostic audio dans Home Assistant.

Le moniteur série réaffiche aussi un résumé toutes les dix secondes lorsqu'il
est connecté ; il n'est plus nécessaire de capturer l'instant du démarrage.
