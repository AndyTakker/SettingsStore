//============================================================ (c) A.Kolesov ===
// Пример хранения пользовательских настроек во flash
// с использованием библиотеки SettingsStore.
//------------------------------------------------------------------------------
#include <SettingsStore.h>
#include <debug.h>

// Пример структуры настроек — последним полем идёт CRC (если используется)
// Сама структура и ее имя могут быть любыми (и вообще может быть не структура,
// а массив или просто переменная).

// Если планируется использование CRC, то два последних байта нужно зарезервировать.
// атрибут "packed" гарантирует, что CRC мы найдем в последних двух байтах
struct __attribute__((packed)) AppConfig {
  uint8_t volume = 7;  // Запомним уровень громкости
  int16_t freq = 1050; // Запомним текущую частоту
  uint8_t idx = 5;     // Запомним номер выбранной частоты в списке частот (на будущее)
  uint16_t crc = 0;    // обязательно последнее, если use_crc = true.
};
struct AppConfig cfg;                                   // Экземпляр структуры с параметрами
SettingsStore settings(&cfg, sizeof(cfg), true, false); // Используем CRC и не пишем, если изменений не было.

// Вспомогательная функция для печати значений настроек
void printConfig(const struct AppConfig *cfg) {
  printf("Volume: %d, Freq: %d, Idx: %d, CRC=0x%04X\r\n", cfg->volume, cfg->freq, cfg->idx, cfg->crc);
}

//==============================================================================
int main(void) {

  // Стандартные приседания
  SystemCoreClockUpdate();
  Delay_Init();
  USART_Printf_Init(115200);

  printf("SystemClk: %ldHz\r\n", SystemCoreClock);
  printf("   ChipID: 0x%08lX\r\n\r\n", DBGMCU_GetCHIPID());

  printf("Config size: %d, CRC position: %d\r\n", sizeof(cfg), sizeof(cfg) - 2);

  // Попытка загрузить настройки с проверкой CRC
  if (!settings.load()) {
    printf("CRC error or first run — initializing defaults\r\n");
    cfg = AppConfig{}; // Устанавливаем настройки по умолчанию
  } else {
    printConfig(&cfg);
  }

  // Изменение параметра
  cfg.volume += 10;
  printf("New volume: %d\r\n", cfg.volume);

  printf("Writing flash, length %d ...\r\n", sizeof(cfg));
  settings.save();
  printf("Saved new volume: %d\r\n", cfg.volume);

  if (settings.load()) { // Прочитаем параметры из flash
    printConfig(&cfg);
  } else {
    printf("CRC error\r\n");
  }

  while (1)
    ;
}
