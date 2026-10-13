# Wallet Takes Control (WTC) — código fuente

Proyecto C++ para Windows con interfaz Win32/Direct3D 11 y Dear ImGui. Esta distribución contiene fuentes y documentación de compilación, no un programa listo para ejecutar.

Úsalo únicamente en entornos autorizados y conforme a las leyes y condiciones de uso aplicables. El software se proporciona sin garantías; consulta [LICENSE](LICENSE).

## Requisitos

- Windows x64.
- Visual Studio o Build Tools con **Desarrollo para el escritorio con C++**, toolset **MSVC v145** y Windows SDK 10.0.
- C++20, configurado en **Release | x64**. Las otras configuraciones heredadas no son el objetivo de esta distribución ni están validadas.
- Dependencias DMA x64 instaladas manualmente según [WTC/lib/dma/README.md](WTC/lib/dma/README.md).

La solución conserva metadatos antiguos de Visual Studio; el requisito efectivo de compilador lo establece `PlatformToolset=v145` en el proyecto. No se ha verificado otro toolset.

## Compilar

1. Obtén las dependencias externas e instala los cinco archivos de desarrollo indicados en [la guía DMA](WTC/lib/dma/README.md).
2. Abre [WTC/sdk.sln](WTC/sdk.sln).
3. Selecciona **Release** y **x64** y compila la solución.

También puedes usar una consola de desarrollador con MSBuild:

```powershell
msbuild WTC\sdk.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

El ejecutable se genera en `WTC/build/x64/Release/` y los archivos intermedios en `WTC/build/obj/x64/Release/`. No publiques esas carpetas con el código fuente. Las dependencias no se descargan automáticamente.

## Ejecutar y configurar

- Copia las DLL y demás dependencias de ejecución indicadas en [la guía DMA](WTC/lib/dma/README.md) junto al ejecutable.
- Se requiere el entorno DMA/dispositivo compatible; compilar correctamente no acredita funcionamiento con hardware ni compatibilidad con una versión actual del juego.
- **F1** abre/oculta el menú; **F2** cambia el preset. El README original especifica configuración de doble PC para esas teclas.
- La configuración se guarda en `%LOCALAPPDATA%\WTC\wtc_menu.cfg`, con alternativa junto al ejecutable si no se puede resolver la carpeta de usuario. No se incluyen perfiles personales.
- Los offsets predeterminados están en [el código](WTC/src/game/offsets.hpp). El menú permite crear `offsets.txt` junto al ejecutable y activar su lectura. No se distribuye la copia antigua de la carpeta de fuentes; actualiza los valores cuando corresponda.
- El renderizador usa Arial Bold instalada en `C:\Windows\Fonts\arialbd.ttf`. No se redistribuye esa fuente. La ubicación está fijada en el código; una instalación de Windows en otra ruta requiere adaptar [el renderizador](WTC/src/utils/render/render.cpp).
- Se conservan mensajes operativos de consola y la generación/reutilización de un mapa de memoria temporal usado por la inicialización DMA. No son logs ni archivos de prueba incluidos en el paquete. No se garantiza ausencia de diagnósticos internos de bibliotecas externas.

## Estructura

```text
LICENSE
README.md
.gitignore
WTC/
  sdk.sln
  sdk.vcxproj
  src/
    core/       Estado y configuración
    features/   Funciones de la aplicación
    game/       Tipos y datos del juego
    gui/        Interfaz
    memory/     Integración DMA
    utils/      Renderizado y utilidades
  lib/
    imgui/      Fuentes necesarias y licencia de Dear ImGui
    dma/        Instrucciones para instalar el SDK externo
```

La distribución excluye pruebas, cachés del IDE, compilados, registros de compilación, instrucciones internas, preferencias personales, ejemplos de ImGui y diagnósticos de matrices X-ray. Los originales de desarrollo no se han modificado.

## Validación de esta distribución

Se realizó una recompilación completa **Release | x64** en una copia aislada, con MSBuild 18.10.1, MSVC 14.51.36231 (v145), las dependencias de desarrollo originales y sin las hojas de propiedades del usuario: **0 errores, 88 advertencias**. Estas incluyen extensiones de los headers DMA, conversiones numéricas y formatos de `printf` del código conservado; no se han ocultado ni corregido dentro de esta limpieza.

Las dependencias usadas para esa comprobación no se incluyen en el paquete público. No se ha probado descargar y reconstruir el SDK desde sus fuentes, ni ejecutar el programa con hardware o con una versión actual del juego. Esta validación no es una auditoría funcional o de seguridad completa.

## Licencias y atribución

- Código del proyecto: [MIT, copyright 2026 Marazm](LICENSE), conservado del original.
- Dear ImGui: [licencia MIT y atribuciones](WTC/lib/imgui/LICENSE.txt). Se conservan también los avisos de sus componentes integrados.
- MemProcFS, LeechCore y FTDI: dependencias externas con condiciones propias, no cubiertas por la licencia MIT del proyecto. Consulta [la guía DMA](WTC/lib/dma/README.md) antes de redistribuir un programa enlazado o sus dependencias. Esta limpieza no certifica compatibilidad jurídica entre todas las licencias.
