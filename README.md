# Nrf-Sync 📡

Nrf-Sync is an application that allows you to toggle high a pin on two different nRF52840 DK boards for a certain amount of time in perfect sync. For this, one of the boards (the transmitter) sends a radio packet and after a configurable amount of time (the offset) toggles the pin high, while the other (the receiver) receives this packet and toggles its pin high as well. The offset is configured so that both pins go high at the exact same time.  

To use the application you need:
- 2 nRF5280 DK boards
- Nordic's nRF5 SDK 17
- Segger Embedded Studio

The repository must be cloned into any folder inside de **examples** folder of the SDK. For example, a working setup would be:
_C:\nRF5\_SDK\examples\my\_folder\nrf-sync_ \*

\*(my_folder is just a folder I created in **examples**, nRF5_SDK is how I renamed the Nordic SDK folder)

To download the code into both boards, open the corresponding Segger Embedded Studio Project that can be found in:
_C:\nRF5\_SDK\examples\my\_folder\nrf-sync\nrf-sync\_receiver\pca10056\blank\ses_
_C:\nRF5\_SDK\examples\my\_folder\nrf-sync\nrf-sync\_transmitter\pca10056\blank\ses_
