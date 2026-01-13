#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <ch32v00x.h>
#include <stdio.h>
#include <string.h>

// === Настройки flash ===
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 64
#endif

#ifndef FLASH_END_ADDR
#define FLASH_END_ADDR 0x08004000U // 16 КБ flash: 0x08000000 + 0x4000
#endif

// Flash Control Register bits
#define CR_PG_Set ((uint32_t)0x00000001)
#define CR_PG_Reset ((uint32_t)0xFFFFFFFE)
#define CR_PER_Set ((uint32_t)0x00000002)
#define CR_PER_Reset ((uint32_t)0xFFFFFFFD)
#define CR_MER_Set ((uint32_t)0x00000004)
#define CR_MER_Reset ((uint32_t)0xFFFFFFFB)
#define CR_OPTPG_Set ((uint32_t)0x00000010)
#define CR_OPTPG_Reset ((uint32_t)0xFFFFFFEF)
#define CR_OPTER_Set ((uint32_t)0x00000020)
#define CR_OPTER_Reset ((uint32_t)0xFFFFFFDF)
#define CR_STRT_Set ((uint32_t)0x00000040)
#define CR_LOCK_Set ((uint32_t)0x00000080)
#define CR_FLOCK_Set ((uint32_t)0x00008000)
#define CR_PAGE_PG ((uint32_t)0x00010000)
#define CR_PAGE_ER ((uint32_t)0x00020000)
#define CR_BUF_LOAD ((uint32_t)0x00040000)
#define CR_BUF_RST ((uint32_t)0x00080000)

// FLASH Status Register bits
#define SR_BSY ((uint32_t)0x00000001)

// FLASH Keys
// Блокировка записи во flash устанавливается одним битом в регистре, а вот снятие блокировки
// разработчики сделали в виде последовательной записи в CTRL-регистр вот таких ключей.
// Видимо, для того, чтобы случайно нельзя было разблокировать запись во flash, т.к. адресное
// пространство общее и запросто можно по ошибке не туда написать.
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)

class SettingsStore {
  private:
  void *settingsBuf;    // Указатель на буфер с данными
  uint32_t address;     // Начальный адрес во flash
  uint32_t length;      // Фактический размер данных (байт).
  uint32_t alignedSize; // Выравненный размер данных кратно странице
  bool useCrc;          // Признак использования CRC
  bool forceWrite;      // Признак записи без проверки на совпадение

  public:
      SettingsStore(void *ptr, size_t length, bool useCrc, bool forceWrite);
      void save(void); // Сохранение структуры в flash.
      bool load(void); // Чтение структуры из flash.

  private:
  size_t align_up(size_t value, size_t alignment);                            // Выравнивание по кратности размера
  uint16_t crc16(const void *data, size_t len);                               // CRC16-CCITT
  uint32_t flashStartAddr(size_t data_size);                                  // Адрес начала данных во flash
  void flashRead(uint32_t addr, uint8_t *buf, size_t len);                    // Чтение данных из flash
  void flashErase(/* size_t size */);                                         // Очистка области flash, выделенной под сохранение настроек.
  void flashWrite(/* uint32_t StartAddr, uint32_t *pbuf, uint32_t Length */); // Запись данных во flash
};

#endif // SETTINGS_STORE_H