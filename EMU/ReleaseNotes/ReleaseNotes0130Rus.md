# Заметки к релизу Dolwin 0.130

Релиз с несчастливым номером 13 был разбит на два релиза: 0.130 и 0.131. Ох уж эти суеверные программисты.

Что нового:
- Поддержка MMU
- Поддержка эмуляции кэша
- Динамический рекомпилятор
- Улучшенная эмуляция графического FIFO
- Много других мелких улучшений

Все эти вещи добавлены экспериментально и в настоящий момент кэш и рекомпилятор временно отключены. Если вы разработчик, то можете пересобрать Dolwin с включенным кэшем и рекомпилятором.

Кэш включается командой `CacheDebugDisable 0`.

Рекомпилятор включается в SRC\\Core\\Gekko.cpp, line 20 (но при этом нужно отключить интерпретатор).

В промежутке между 0.130 и 0.131 я постараюсь исправить все непонятные баги с кэшем и рекомпилятором, чтобы они были включены в следующем релизе.

## Требования 

- Dolwin интенсивно использует многоядерную многопоточность. Поэтому желательно чтобы ваш процессор содержал 4 или более ядер.
- Требования к памяти не такие жесткие, нескольких гигабайт должно хватить
- Для эмуляции требуются дампы DSP IROM/DROM
- Дамп образа BIOS не требуется, но если вы хотите с ним поэкспериментировать, то его можно тоже добавить в настройках. Запускается BIOS через меню File -> Run Bootrom. Затем нужно немного подождать и открыть крышку привода (File -> Swap Disk -> Open Cover). После этого запустится IPL Menu :p

## Что получается

В целом эмуляция GameCube значительно продвинулась вперед. Запускаются такие игры как Ikaruga, 18 Wheeler, Super Monkey Ball и например Ed, Edd and Eddy.

Однако не все они доходят до Ingame, содержат графические баги и страдают тормозами.

Большая часть игр по прежнему не запускается из-за недостаточно точной эмуляции DSP или графического процессора.

Следующий релиз (0.131) нацелен как раз на устранение всех недочетов эмуляции DSP, чтобы наконец можно было запустить такие топовые игры как Legend of Zelda.