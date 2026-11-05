# UnlockRacesClasses

AzerothCore fun module:

- **Разблокировка всех комбинаций раса/класс** — включая рыцаря смерти для всех рас.
- **Смена расы/класса в игре** через NPC-мастера (gossip-меню).
- Заготовка под кастомные специализации: **Blademaster**, **Necromancer**.

## Установка

1. Клонируйте модуль в `modules/` вашего AzerothCore:

```
cd azerothcore-wotlk/modules
git clone <this-repo> unlock-races-and-classes
```

2. Пересоберите ядро (модуль подключится автоматически через CMake).
3. Примените SQL: `data/sql/db-world/unlock-races-classes.sql` (или добавьте NPC вручную).
4. При необходимости скопируйте `conf/UnlockRacesClasses.conf.dist` в `env/dist/etc/` и настройте.

## Настройка

| Опция | Описание | По умолчанию |
|---|---|---|
| `UnlockRacesClasses.Enable` | Вкл/выкл модуль | `1` |
| `UnlockRacesClasses.NpcEntry` | Entry NPC-мастера | `95000` |
| `UnlockRacesClasses.MinLevel` | Минимальный уровень для любой смены (0 = без ограничения) | `10` |
| `UnlockRacesClasses.EnableNewRaces` | Показ Cata+/MoP рас (Гоблин, Ворген, Пандарен) в меню. Только с клиентским патчем! | `0` |
| `UnlockRacesClasses.TeleportOnRaceChange` | Телепорт к стартовой локации новой расы (таблица `mod_urc_start_locations`) | `1` |
| `UnlockRacesClasses.GiveStartItems` | Выдавать стартовые предметы (таблица `mod_urc_start_items`) после смены расы/класса. Выдаётся один раз на персонажа | `1` |
| `UnlockRacesClasses.ChangeCooldown` | Кулдаун в секундах между сменами (0 = без кулдауна) | `0` |
| `UnlockRacesClasses.RaceChange.ItemEntry/ItemCount` | Цена смены расы (предмет, количество; 0 = бесплатно) | `0` |
| `UnlockRacesClasses.ClassChange.ItemEntry/ItemCount` | Цена смены класса | `0` |
| `UnlockRacesClasses.SpecChange.ItemEntry/ItemCount` | Цена выбора специализации | `0` |
| `UnlockRacesClasses.Spec.Blademaster.Spells/Auras` | Спеллы/ауры спека Blademaster (через запятую) | пример |
| `UnlockRacesClasses.Spec.Necromancer.Spells/Auras` | Спеллы/ауры спека Necromancer (через запятую) | пример |

## Использование

Поговорите с NPC «Мастер Перевоплощений» (entry 95000). NPC стоят во всех 8 столицах:

- **Альянс:** Штормвинд, Стальгорн, Дарнас, Экзодар
- **Орда:** Оргриммар, Подгород, Громовой Утёс, Луносвет

Доступные действия:

1. **Сменить расу** — с учётом уровня, цены из конфига и состояния игрока (вне боя, живой, не на такси)
2. **Сменить класс** — с учётом уровня, цены и состояния игрока
3. **Выбрать специализацию** — Blademaster или Necromancer: спеллы учатся навсегда, ауры накладываются и восстанавливаются при каждом входе в игру

Команды:

- `.urc race <id>` / `.urc class <id>` — административные команды (только для администраторов, игнорируют кулдаун)
- `.urcui status` / `.urcui race <id>` / `.urcui class <id>` / `.urcui spec <id>` — команды для UI-аддона (проверки уровня/цены/кулдауна применяются)

> После смены расы/класса рекомендуется перезайти персонажем для корректного обновления моделей и спеллов.
>
> Спеллы/ауры кастомных специализаций настраиваются в конфиге — подберите нужные ID под свой сервер.

## Совместимость сервер / база / клиент

Модуль рассчитан на AzerothCore + клиент 3.3.5a:

- Меню NPC показывает только расы/классы, которые клиент способен отобразить (MoP/Legion контент скрыт)
- Специализации хранятся в **characters**-БД (`mod_unlock_races_classes_specs`) и восстанавливаются при входе
- Стартовые предметы выдаются **один раз** на персонажа (отметка в characters-БД: `mod_unlock_races_classes_start_items`)

## Установка SQL

| Файл | База |
|---|---|
| `data/sql/db-world/unlock-races-classes.sql` | **world** — NPC и спавны |
| `data/sql/db-world/unlock-races-classes-start-locations.sql` | **world** — стартовые локации рас |
| `data/sql/db-world/unlock-races-classes-start-items.sql` | **world** — стартовые предметы |
| `data/sql/characters/unlock-races-classes.sql` | **characters** — персистентность специализаций |
| `data/sql/characters/unlock-races-classes-start-items.sql` | **characters** — отметка выдачи стартовых предметов |

Подробности и чек-лист: [docs/CLIENT-COMPATIBILITY.md](docs/CLIENT-COMPATIBILITY.md)
