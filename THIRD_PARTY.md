Les pilotes `src/board/` proviennent des exemples Arduino 01 et 03 du dépôt
Waveshare https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C
au commit `8ead4a96bf3a278fc4ebd8ef4768657e17fa2880` (licence Apache-2.0,
reproduite dans `src/board/LICENSE`). Les mentions Espressif sont conservées.
`Display_ST77916.cpp` a été adapté : synchronisation DMA, fermeture de l'interface
de sondage, suppression du test de démarrage, initialisation du résultat de lecture
et correction de la valeur PWM maximale.
LVGL 9.3.0 (MIT) et PubSubClient 2.8 (MIT) sont téléchargés par PlatformIO.

`es8311.cpp` vérifie aussi l’allocation du handle avant de l’utiliser.
