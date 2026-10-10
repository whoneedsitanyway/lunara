# Разработка

## Требования

- macOS (сборка проверена на Apple Silicon, macOS 27, Apple Clang 21).
- Command Line Tools (или Xcode) — `xcode-select -p`.
- Python 3 (для локального venv с cmake/ninja).
- CMake ≥ 3.22 и Ninja — устанавливаются локально в `~/Lunara/.tools`
  (в систему ничего не ставится).

## Тулчейн (локальный)

```bash
cd ~/Lunara
python3 -m venv .tools
.tools/bin/pip install --upgrade pip
.tools/bin/pip install cmake ninja
```

JUCE подтягивается через CMake `FetchContent` в `build/_deps/juce-src`
(версия зафиксирована в `CMakeLists.txt` — сейчас 8.0.6).

## Конфигурация и сборка

```bash
cd ~/Lunara
.tools/bin/cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$HOME/Lunara/.tools/bin/ninja"
.tools/bin/cmake --build build --config Release
```

Повторная сборка после правок кода — только вторая команда.
После правок `CMakeLists.txt` — снова обе.

## Установка

`COPY_PLUGIN_AFTER_BUILD TRUE` в `CMakeLists.txt` автоматически копирует
артефакты:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/Lunara.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/Lunara.component`
- Standalone → `build/Lunara_artefacts/Release/Standalone/Lunara.app`

## Проверка AU

```bash
auval -v aufx Luna Strv      # должно быть: AU VALIDATION SUCCEEDED
```

Коды: тип `aufx`, подтип `Luna`, производитель `Strv`.

## Карта кода

```
Source/
  PluginProcessor.h/.cpp      процессор: параметры, BPM-синк, цепь, randomize
  PluginEditor.h/.cpp         UI: корпус педали, раскладка, привязки, тема
  PresetManager.h             файловые пресеты (save/load XML-состояния)
  FactoryPresets.h            вшитые заводские пресеты (XML-строки)
  DebugLogger.h/.cpp          «чёрный ящик»: снапшоты уровней, аномалии, лог-файл
  dsp/
    GrainEngine.h             гранулярный движок
    SyncDelay.h               синхронный дилей (2-полюсный Tone)
    CloudReverb.h             FDN cloud reverb
    Limiter.h                 выходной лимитер (вкл/выкл, мягкий потолок)
    HighPassFilter.h          выходной low-cut (HPF, 20..500 Гц)
  ui/
    LunaraLookAndFeel.h/.cpp  LookAndFeel, темы, IconButton
    WaveDisplay.h/.cpp        осциллограф + индикатор GR/лимитирования + FPS
Tests/
  DspStressTest.cpp           офлайн стресс-тест DSP (конечность/ограниченность)
  PresetTest.cpp              round-trip тест пресетов (save → change → load)
  AutoGainTest.cpp            проверка совпадения уровня выхода и входа
  DriftTest.cpp               проверка медленного питч-дрейфа (не jitter)
  HalftimeTest.cpp            проверка удвоения времени дилея в HALF TIME
  RunawayTest.cpp             защита выхода от разгона + работа чёрного ящика
  ThemeRenderTest.cpp         рендер светлой темы: контраст текста контролов
  ParamFormatTest.cpp         значения параметров показывают ≤ 4 знаков
  FxSourceTest.cpp            коммутация эффектов IN/OUT/BOTH остаётся ограниченной
  FactoryPresetTest.cpp       вшитые пресеты + выбор "Play with Me" по умолчанию
  HpfTest.cpp                 HPF-ручка: дефолт 100 Гц, режет низ, не трогает верх
  FreezeTest.cpp              Freeze удерживает облако на тишине (без него — затухает)
  StretchReverseTest.cpp      REVERSE разворачивает облако; STRETCH тянет хвост
doc/                          эта документация
```

## Тесты

Все офлайн-тесты собираются вместе с плагином (`cmake --build build`) или
поимённо: `--target LunaraDspTest LunaraHpfTest ...`. Таргеты создаются
хелперами `lunara_add_dsp_test` / `lunara_add_processors_test` /
`lunara_add_plugin_test` в `CMakeLists.txt` — новый тест достаточно добавить
одной строкой.

Запуск всех тестов:

```bash
cd ~/Lunara
for t in DspTest PresetTest DriftTest HalftimeTest RunawayTest AutoGainTest \
         ThemeRenderTest ParamFormatTest FxSourceTest FactoryPresetTest \
         HpfTest FreezeTest StretchReverseTest; do
    "./build/Lunara${t}_artefacts/Release/Lunara${t}" || exit 1
done
```

## Отладка / чёрный ящик

Редкие баги (скачок громкости, писк, шум) ловятся через `DebugLogger`:
аудио-поток пишет снапшоты в lock-free FIFO, фон-поток ведёт лог
`~/Library/Application Support/Lunara/lunara_debug.log`. При аномалии в лог
выгружается история последних 256 блоков. Открыть лог — кнопка **LOG** в хедере
(красный бейдж = аномалия записана). Подробнее — в
[ARCHITECTURE.md](ARCHITECTURE.md), раздел «Диагностика».

- `LunaraDspTest` гоняет гранулы → дилей → реверб → лимитер на экстремальных
  настройках и проверяет, что выход всегда конечен, ограничен и не «горячий»
  (защита от «ухода в шум»).
- `LunaraPresetTest` сохраняет состояние, меняет параметры, загружает пресет
  обратно и сверяет значения (во временном каталоге).
- `LunaraAutoGainTest` запускает реальный процессор и проверяет, что с Auto Gain
  уровень выхода совпадает с входом, а без него — ниже.

## Как добавить параметр

1. Добавить id в `lunara::ids` (`PluginProcessor.h`).
2. Создать параметр в `createParameterLayout()` (`PluginProcessor.cpp`).
3. Добавить указатель в `RawParams` (`PluginProcessor.h`) и биндинг в
   `bindRawParams()` — в `processBlock` параметр читается как
   `rawParams.<имя>->load()` (без строковых лукапов в audio-потоке).
4. Добавить контрол в редакторе (`addKnob`/`addCombo`) и в `resized()`.
5. Обновить [PARAMETERS.md](PARAMETERS.md).

## Полезно

- `randomizeParameters()` — пример программного изменения параметров через
  `setValueNotifyingHost`.
- `kModes` — таблица характеров режимов (множители/добавки).
- `kDivisions` — таблица музыкальных делений.
