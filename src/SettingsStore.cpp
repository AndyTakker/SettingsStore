//============================================================= (c) A.Kolesov ==
// SettingsStore.cpp
// Библиотека для работы с настройками — сохранение/чтение настроек в/из flash
//
// Микроконтроллеры серии CH32V003 не имеют на борту EEPROM, поэтому ее приходится
// эмулировать с помощью flash.
// Минусы такого подхода:
// - отъедается и так небольшой размер памяти под программу.
// - при обновлении прошивки все ранее сохраненные во flash данные будут потеряны.
//
// Библиотека ориентирована на работу с пользовательскими настройками и конфигурационными
// параметрами устройства и не предполагает активной перезаписи содержимого flash,
// поэтому в ней не предусмотрен wear-leveling (хотя небольшую оптимизацию записи можно
// подключить).
// Если нужно писать часто, лучше использовать другую библиотеку, а еще лучше - внешнюю EPPROM.
//
// Особенности:
// - Данные размещаются от конца flash вниз.
// - Используется Fast mode постраничный, поэтому сохраняемый размер округляется
//   до значения, кратного размеру страницы (64 байта).
// - Поддержка CRC16-CCITT (опционально). Использует последние 2 байта в структуре).
// - Нет динамического выделения памяти.
// Массив сохраняемых данных (может быть оформлен в виде структуры) должен иметь фиксированный размер.
// Структура должна быть описана с __attribute__((packed)), если планируется использование CRC.
// Место под хранение вычисляется автоматически по размеру структуры и кратно размеру страницы flash.
//
// Если требуется высокая достоверность данных, можно подключить контроль данных с использованием CRC,
// но использование CRC немного снижает скорость работы (+8 мксек на операцию чтения/сохранения).
//------------------------------------------------------------------------------

#include "SettingsStore.h"

//==============================================================================
// Конструктор:
//  @param ptr         указатель на структуру
//  @param length      размер структуры в байтах (используй sizeof())
//  @param useCrc      true: последние 2 байта заполняются CRC16 перед записью
//  @param forceWrite  true: запись без проверки, что данные изменились
//------------------------------------------------------------------------------
SettingsStore::SettingsStore(void *ptr, size_t length, bool useCrc, bool forceWrite)
    : settingsBuf(ptr),
      length(length),
      useCrc(useCrc && length >= 2),
      forceWrite(forceWrite) {
  this->alignedSize = (uint32_t)align_up((size_t)length, (size_t)FLASH_PAGE_SIZE);
  this->address = flashStartAddr(alignedSize);
}

//==============================================================================
// Чтение данных из flash
//  @param ptr     указатель на структуру, в которую поместим прочитанные данные
//  @param size    размер структуры в байтах (используй sizeof())
//  @param use_crc true: проверять CRC. CRC хранится в двух последних байтах структуры.
//  @return        true при успехе, false при ошибке CRC
//------------------------------------------------------------------------------
bool SettingsStore::load() {

  flashRead(this->address, (uint8_t *)this->settingsBuf, this->length);
  if (!this->useCrc) { // CRC не используем
    return true;
  }
  // Извлекаем CRC из последних 2 байт прочитанного массива
  uint16_t stored_crc;
  memcpy(&stored_crc, (uint8_t *)this->settingsBuf + this->length - 2, 2);
  // Считаем CRC от всех байт, кроме последних 2
  uint16_t computed_crc = crc16(this->settingsBuf, this->length - 2);

  return (stored_crc == computed_crc);
}

//==============================================================================
// Сохранение массива данных во flash
//------------------------------------------------------------------------------
void SettingsStore::save() {
  // Проверка, изменились ли данные (без учёта CRC, если используется)
  // Это такая "экономия на спичках" ресурса flash - если данные, которые хотим записать,
  // не отличаются от тех, что уже записаны, то можно не записывать.
  // Это кейс, например, когда опрашиваем пользовательский ввод (настройки) и сохраняем,
  // а пользователь много чего понажимал, но по факту параметры не изменились.
  //
  if (!this->forceWrite) {
    bool changed = false;
    size_t compare_size = this->useCrc ? (this->length - 2) : this->length;
    for (size_t i = 0; i < compare_size; ++i) {
      uint8_t flash_byte;
      flashRead(this->address + i, &flash_byte, 1);
      if (flash_byte != ((uint8_t *)this->settingsBuf)[i]) {
        changed = true;
        break;
      }
    }
    if (!changed) {
      return; // Ранее сохраненные во flash данные не отличаются от сохраняемых
    }
  }

  // Подставляем CRC в последние 2 байта, если CRC используется
  if (this->useCrc) {
    uint16_t crc = crc16(this->settingsBuf, this->length - 2);
    memcpy((uint8_t *)this->settingsBuf + this->length - 2, &crc, 2);
  }

  flashErase(); // Стирание всех задействованных страниц
  flashWrite(); // И запись
  return;
}

// ******************** Вспомогательные функции ********************

//==============================================================================
// Выравнивание размера вверх кратно заданному значению.
//  @param value     - исходный размер
//  @param alignment - значение выравнивания (например, 64 для страницы flash)
//------------------------------------------------------------------------------
size_t SettingsStore::align_up(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

//==============================================================================
// Вычисление CRC16-CCITT (полином 0x1021, начальное значение 0xFFFF)
//  @param data      — указатель на массив данных, для которых считаем CRC.
//  @param size      — размер массива (используй sizeof())
//  @return          — рассчитанная CRC
//------------------------------------------------------------------------------
uint16_t SettingsStore::crc16(const void *data, size_t len) {
  uint16_t crc = 0xFFFF;
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)(p[i]) << 8;
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

//==============================================================================
// Адрес начала данных во flash (от конца flash вниз, выровнено по страницам)
//------------------------------------------------------------------------------
uint32_t SettingsStore::flashStartAddr(size_t data_size) {
  size_t pages = (data_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
  return FLASH_END_ADDR - pages * FLASH_PAGE_SIZE;
}

// ******************** LOW-LEVEL Функции работы с flash ********************

//==============================================================================
// Непосредственное чтение данных из flash в буфер
//  @param addr - начальный адрес во flash
//  @param buf - буфер для читаемых данных
//  @param len - количество читаемых данных
//------------------------------------------------------------------------------
void SettingsStore::flashRead(uint32_t addr, uint8_t *buf, size_t len) {
  const uint8_t *flash_ptr = (const uint8_t *)addr;
  for (size_t i = 0; i < len; ++i) {
    buf[i] = flash_ptr[i];
  }
}

//==============================================================================
// Запись данных во flash.
//------------------------------------------------------------------------------
void SettingsStore::flashWrite() {
  uint32_t *pbuf = (uint32_t *)this->settingsBuf; // Указатель на буфер с данными
  uint32_t align_size = this->alignedSize;        // Выравненый по размеру страницы размер
  uint32_t pageAdr = this->address;               // Адрес начала страницы
  uint32_t startAddr = this->address;             // Адрес слова на странице
  uint32_t cntPage = align_size >> 6;             // Кол-во страниц flash
  uint32_t cntWord = (this->length + 3) >> 2;     // Счетчик количества записанных 4-хбайтных слов

  // Разблокировка записи во flash
  FLASH->KEYR = FLASH_KEY1;
  FLASH->KEYR = FLASH_KEY2;

  // Разблокировка Fast Programming
  FLASH->MODEKEYR = FLASH_KEY1;
  FLASH->MODEKEYR = FLASH_KEY2;

  do {
    FLASH->CTLR |= CR_PAGE_PG; // Режим записи постранично
    FLASH->CTLR |= CR_BUF_RST; // Сброс буфера
    while (FLASH->STATR & SR_BSY)
      ;
    uint8_t cnt = FLASH_PAGE_SIZE >> 2; // 16 - кол-во 4-х байтных слов на странице flash
    uint32_t val;
    while (cnt) {
      // val = *(uint32_t *)pbuf;
      // if (cntWord == 0) { // Все данные записаны во flash, добиваем страницу "пустышками"
      //   val = 0xFFFFFFFF;
      // } else {
      //   cntWord--;
      //   pbuf++; // Берем следующее слово из входного буфера
      // }
      if (cntWord > 0) {
        val = *(uint32_t *)pbuf;
        pbuf++;
        cntWord--;
      } else {
        val = 0xFFFFFFFF;
      }

      *(__IO uint32_t *)(startAddr) = val;
      FLASH->CTLR |= CR_BUF_LOAD; // Перенос даных из буфера непосредственно во flash.
      while (FLASH->STATR & SR_BSY)
        ;
      startAddr += 4; // Переход к начальному адресу следующего записываемого слова
      cnt--;
    }

    FLASH->CTLR |= CR_PAGE_PG;
    FLASH->ADDR = pageAdr;
    FLASH->CTLR |= CR_STRT_Set;
    while (FLASH->STATR & SR_BSY)
      ;
    FLASH->CTLR &= ~CR_PAGE_PG;

    pageAdr += FLASH_PAGE_SIZE; // Переход к начальному адресу следующей страницы
  } while (--cntPage);

  FLASH->CTLR |= CR_FLOCK_Set;
  FLASH->CTLR |= CR_LOCK_Set;

  return;
}

//==============================================================================
// Стирание области flash, выделенной под сохранение настроек
//------------------------------------------------------------------------------
void SettingsStore::flashErase() {
  uint32_t addr = this->address;
  uint32_t erase_size = this->alignedSize;

  // Разблокировка flash для записи
  FLASH->KEYR = FLASH_KEY1;
  FLASH->KEYR = FLASH_KEY2;

  // Разблокировка режима Fast program mode
  FLASH->MODEKEYR = FLASH_KEY1;
  FLASH->MODEKEYR = FLASH_KEY2;

  uint32_t startAddr = addr;      // Адрес начала стирания
  uint32_t cnt = erase_size >> 6; // Кол-во стираемых страниц flash
  do {                            // Стираем постранично
    FLASH->CTLR |= CR_PAGE_ER;    // Включение режима быстрого (постраничного) стирания
    FLASH->ADDR = startAddr;      // Адрес начала стирания
    FLASH->CTLR |= CR_STRT_Set;   // Запуск стирания
    while (FLASH->STATR & SR_BSY) // Ждем окончания стирания
      ;
    FLASH->CTLR &= ~CR_PAGE_ER;   // Выключение режима быстрого (постраничного) стирания
    startAddr += FLASH_PAGE_SIZE; // Переходим к адресу следующей страницы
  } while (--cnt);

  FLASH->CTLR |= CR_FLOCK_Set; // Блокируем запись во flash
  FLASH->CTLR |= CR_LOCK_Set;

  return;
}
