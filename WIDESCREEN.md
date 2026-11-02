# Установленная графическая основа

Установлен архив `PvZWidescreenForSteam.zip` из Downloads — выпуск
Twig6943/PvZWidescreenSteam 1.0.

Архив заменяет основной EXE и main.pak, добавляет pvzgame.exe и меняет
несколько properties-файлов. Данные установки и контрольные суммы записаны
в `widescreen-installed.json`; там же указан каталог резервной копии.
Пользовательские сохранения установщик не менял.

## Запуск

Запускайте оригинальную запись PvZ в Steam через `steam-launch.sh %command%`.
При наличии `multiplayer/wide-menu-enabled` загрузчик включает нативную DLL
меню и LAN-лобби. Если убрать этот маркер при наличии widescreen-installed.json,
скрипт передаст исходную команду Steam widescreen-загрузчику без DLL.

Кнопка, игровая рамка и полноэкранный режим подтверждены скриншотами пользователя.
См. [NATIVE_MENU.md](NATIVE_MENU.md) и [LAN.md](LAN.md).

## Возврат к предыдущей установке

Закройте игру. Из каталога резервной копии, указанного в JSON, восстановите
файлы с их исходными относительными путями. Удалите добавленные `pvzgame.exe`
и `eula.txt` только если manifest.json отмечает `existed: false`. Затем уберите
`multiplayer/widescreen-installed.json`, чтобы вернуть прежнюю цепочку загрузки.
