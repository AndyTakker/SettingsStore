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
  uint32_t device_id = 0;
  int16_t gain = 0;
  uint8_t mode = 0;
  uint8_t status = 0;
  uint32_t freq_value = 0;
  uint8_t freq_form = 0;
  uint16_t dummy[40] = {0};
  uint16_t crc = 0; // обязательно последнее, если use_crc = true.
};

// Вспомогательная функция для печати значений настроек
void printConfig(const struct AppConfig *cfg) {
  printf("id=0x%08lX, gain=%d, mode=%d, status=%d, freq_value=%d, freq_form=%d crc=0x%04X\r\n",
         (unsigned long)cfg->device_id, cfg->gain, cfg->mode, cfg->status, cfg->freq_value, cfg->freq_form, cfg->crc);
}

//==============================================================================
int main(void) {

  struct AppConfig cfg;                                    // Экземпляр структуры с параметрами
  SettingsStore settings(&cfg, sizeof(cfg), true, false); // Объект для работы с параметрами

  // Стандартные приседания
  SystemCoreClockUpdate();
  Delay_Init();
  Delay_Ms(5000); // Чтобы дождаться пока раздуплится UART-монитор
  USART_Printf_Init(115200);

  printf("SystemClk: %dHz\r\n", SystemCoreClock);
  printf("   ChipID: 0x%08x\r\n\r\n", DBGMCU_GetCHIPID());

  printf("Config size: %d, CRC position: %d\r\n", sizeof(cfg), sizeof(cfg) - 2);

  // Попытка загрузить настройки с проверкой CRC
  if (!settings.load()) {
    printf("CRC error or first run — initializing defaults\r\n");
    cfg.device_id = 0x12345678;
    cfg.gain = 100;
    cfg.mode = 1;
    cfg.status = 2;
    cfg.freq_value = 1234567890;
    cfg.freq_form = 3;
  } else {
    printConfig(&cfg);
  }

  // Изменение параметра
  cfg.gain += 10;
  printf("New gain: %d\r\n", cfg.gain);

  printf("Writing flash, length %d ...\r\n", sizeof(cfg));
  settings.save();
  printf("Saved new gain: %d\r\n", cfg.gain);

  if (settings.load()) { // Прочитаем параметры из flash
    printConfig(&cfg);
  } else {
    printf("CRC error\r\n");
  }

  while (1)
    ;
}
