# ZCanFeedbackBiDiSTM32
Feedback decoder for BiDi with ZCan interface to Z21 based on STM32

This project is based on the work of:
- Philipp Gathow (https://pgahtow.de/w/R%C3%BCckmeldung 8xRailCom-Detektor)
- Zimo Can Protocol

Provides Z21 and Roco 10808 compatible Bidi/Railcom detector.

Currently Railcom does not work because Esp32 Dev Kit C does not have even 8 analog inputs accessable via DMA
