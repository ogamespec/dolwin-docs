# Gekko Locked Cache

This section contains a description of the alien Locked cache mechanism ("LCEnable" in Dolphin SDK).

Sources: IBM Gekko RISC Microprocessor User’s Manual (Chapter 9); US Patent 6859862 "METHOD AND APPARATUS FOR SOFTWARE MANAGEMENT OF ON-CHIP CACHE"

## Overview

В нормальных условия DCache работает как 32 Kbyte 8-way-associative кэш с физической адресацией.

При включении LC DCache превращается в 16 Kbyte 4-way обычный кэш + 16 Kbyte locked cache. После этого приложение может самостоятельно управлять данными в locked cache.

## Программный интерфейс

- HID2\[LCE]\: 1: включить Locked cache
- Инструкция dcbz_l используется для аллокации cache lines (32 байтовый блок) в locked cache. Обычно это `dcbz_l 0xE0000000`. Чтобы MMU не выдавал исключение нужно предварительно замаппить виртуальный адрес 0xE0000000 используя DBAT регистры (адрес 0xE0000000 используется в Dolphin SDK по умолчанию для locked cache)
- Для деаллокации используются инструкции dcbi, dcbf; также если закончились cache lines, то используя pseudo-LRU инструкция переаллоцирует новый тег.
