# Lunara — гранулярный cloud delay/reverb

> Рабочее название проекта: **Lunara**. Идея — «среднее между Chase Bliss MOOD MK2
> и Hologram Microcosm»: гранулярный движок, который превращает любой вход в
> дрейфующие эмбиент-текстуры, плюс синхронный дилей, большой «облачный» реверб
> и защитный выходной каскад (лимитер).

## Что это

Lunara — это **мультиэффект для эмбиента**: гранулярный ресемплер + delay + cloud
reverb + limiter. Один вход, один выход, глобальный dry/wet. Управление
сделано в духе педалей Chase Bliss / Hologram Microcosm: минимум контролов,
режимы-«характеры», BPM-синк от хоста и кнопка **Random** для мгновенного поиска
идей.

## Текущий статус

- Версия: `0.1.0` (первый рабочий каркас, всё собирается и валидируется).
- Форматы: **VST3**, **AU**, **Standalone**.
- AU проходит `auval` (`AU VALIDATION SUCCEEDED`).
- Реализованы: гранулярный движок, sync-delay (2-полюсный Tone), cloud reverb
  (FDN), limiter (вкл/выкл), **HPF** (low-cut 20..500 Гц), dry/wet, BPM-синк,
  6 режимов, ручка CHAOS, **HALF TIME**
  (всё в два раза медленнее), кнопка Random (не трогает Mix/Chaos), **Auto Gain**
  с блокировкой OUTPUT, **Freeze** (удержание облака), **FX SOURCE** (коммутация
  эффектов IN / OUT / BOTH по мотивам MOOD MK2), pedal-style UI, компактный режим,
  осциллограф (окно в тактах) с таймингами, индикатором GR/LIMIT и счётчиком FPS,
  **пресеты** (включая вшитые заводские, по умолчанию — `Play with Me`),
  **светлая/тёмная тема**, английские подсказки.
- **Чёрный ящик (DebugLogger)**: журнал уровней по каскадам с автовыгрузкой
  истории при аномалии (`~/Library/Application Support/Lunara/lunara_debug.log`),
  кнопка **LOG** в хедере и красный индикатор пойманного сбоя.

## Быстрый старт

```bash
cd ~/Lunara
.tools/bin/cmake --build build --config Release
```

Артефакты и места установки:

| Формат     | Путь                                                                 |
|------------|----------------------------------------------------------------------|
| VST3       | `~/Library/Audio/Plug-Ins/VST3/Lunara.vst3`                          |
| AU         | `~/Library/Audio/Plug-Ins/Components/Lunara.component`               |
| Standalone | `build/Lunara_artefacts/Release/Standalone/Lunara.app`               |

Подробности по сборке — в [DEVELOPMENT.md](DEVELOPMENT.md).

## Документация

- [ARCHITECTURE.md](ARCHITECTURE.md) — сигнальная цепь, модули DSP, карта файлов.
- [PARAMETERS.md](PARAMETERS.md) — все параметры, режимы и деления синка.
- [DEVELOPMENT.md](DEVELOPMENT.md) — тулчейн, сборка, как добавить параметр.
- [ROADMAP.md](ROADMAP.md) — что сделано и что дальше.

## Лицензия / происхождение

Код полностью оригинальный. Взят только *визуальный и UX-стиль* педалей
(Chase Bliss / Hologram), прошивки которых закрыты и не использовались.
