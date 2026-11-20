#pragma once

void wifi_connect();    // начать подключение (не блокирует)
void wifi_tick();       // вызывать в loop()
bool wifi_connected();
