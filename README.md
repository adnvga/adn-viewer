# ADN Viewer

Visor de PNG minimalista para Windows. Abre una imagen a pantalla completa, permite recorrer los otros PNG de la misma carpeta y descartar imágenes rápidamente a la Papelera de reciclaje.

## Qué hace

- Abre un PNG pasado como argumento, por ejemplo al asociar los archivos `.png` con `adn-viewer.exe` desde **Abrir con** de Windows Explorer.
- Muestra una ventana sin bordes a pantalla completa en el monitor principal.
- Ajusta la imagen para que quepa completa, manteniendo su proporción, sobre un fondo negro.
- Cambia de imagen sin mostrar un fotograma negro intermedio.
- Encuentra los PNG de la carpeta de la imagen abierta y los ordena por fecha de modificación, de más reciente a más antigua. Si hay empate, los ordena por nombre.
- Envía la imagen actual a la Papelera de reciclaje al pulsar `Del`.

## Controles

| Tecla | Acción |
| --- | --- |
| `Izquierda` | Muestra el PNG más reciente de la lista. |
| `Derecha` | Muestra el PNG más antiguo de la lista. |
| `Del` | Mueve la imagen actual a la Papelera y muestra la siguiente; al borrar la última, muestra `No image found`. |
| `Esc` | Cierra el visor. |

Al borrar una imagen situada al final de la lista, el visor vuelve a la primera disponible.

> **Atención:** `Del` mueve el archivo a la Papelera sin pedir confirmación. Aun así, el archivo se puede restaurar desde la Papelera mientras no se vacíe.

## Límites intencionales

- Solo busca y navega archivos PNG. No es un visor general de imágenes.
- El orden es propio: fecha de modificación descendente y, en caso de empate, nombre. Windows Explorer no proporciona al programa asociado su orden visual actual, por lo que el visor no puede heredar automáticamente columnas, agrupaciones, filtros u orden manual de Explorer.
- La lista de imágenes se calcula al abrir el visor. Archivos agregados, eliminados o modificados por otros programas durante esa sesión no actualizan la lista automáticamente.
- No incluye zoom, desplazamiento, rotación, presentación automática, miniaturas, metadatos ni soporte para animaciones PNG.
- Se abre en el monitor principal y no incorpora controles de ventana, ratón ni pantalla táctil.

## Uso

1. Compila el proyecto o descarga `adn-viewer.exe`.
2. En Windows Explorer, elige **Abrir con** para un PNG y selecciona `adn-viewer.exe`.
3. Marca la opción para usarlo siempre si quieres dejar la asociación predeterminada.
4. Abre cualquier PNG con doble clic.

También puede iniciarse desde una terminal:

```powershell
.\adn-viewer.exe "C:\ruta\a\imagen.png"
```

## Compilación

Requisitos:

- Windows.
- MinGW con `g++.exe` y GDI+ disponibles. El proyecto se ha probado con MinGW.org GCC 6.3.0 instalado en `C:\MinGW`.

En VS Code, usa `Ctrl+Shift+B`. La tarea incluida genera `adn-viewer.exe` como aplicación de Windows y enlaza estáticamente los runtimes de MinGW para que pueda abrirse con doble clic sin depender de `C:\MinGW\bin` en `PATH`.

Para compilar desde PowerShell:

```powershell
$env:PATH = "C:\MinGW\bin;$env:PATH"
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o adn-viewer.exe -mwindows -static -static-libgcc -static-libstdc++ -lgdiplus
```

