#pragma once
class ConfigSettings;class NTPSettings;class WifiSettings;class IPSettings;class MQTTSettings;class EthernetSettings;
struct transceiver_config_t;
bool validSettings(const ConfigSettings &);
bool validSettings(const NTPSettings &);
bool validSettings(const WifiSettings &);
bool validSettings(const IPSettings &);
bool validSettings(const MQTTSettings &);
bool validSettings(const EthernetSettings &);
bool validSettings(const transceiver_config_t &);
