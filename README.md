# Wallet Takes Control (WTC) — v4.1

Versión **4.1 —Air-to-Air Missiles**.

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

## ESP de proyectiles vivos (experimental)

En **ESP → General**, activa **Live Projectile ESP (experimental ECS)**; **Projectile Debug** habilita el diagnóstico de ambas consultas una vez por segundo. Ambos controles empiezan desactivados y no se guardan en la configuración.

El lector interpreta `g_RocketListIndexPtr` y `g_BombListIndexPtr` como **selectores de consulta ECS**, no como contadores: resuelve el registro de consulta, lee la matriz de desplazamientos de componentes y recorre las sublistas para obtener, por cada descriptor, la columna de punteros y la columna de estado `active`. Solo se dibujan filas activas. El código del decodificador está en [projectile_ecs.hpp](WTC/src/features/esp/projectile_ecs.hpp) y la integración con el juego en [projectile_esp.hpp](WTC/src/features/esp/projectile_esp.hpp).

Se dibuja la posición actual y el segmento `pos → pos + vel * 0.5s`, naranja para cohetes y rojo para bombas, con el nombre del proyectil junto al marcador cuando se puede resolver. Es una extrapolación recta instantánea, no una curva balística. Un candidato solo se dibuja tras confirmar movimiento continuo entre dos muestras; si desaparece de la consulta o deja de ser coherente, se purga para evitar fantasmas.

**Coste por fotograma:** la consulta ECS se recorre como máximo cada **100 ms**, no en cada fotograma, y el trabajo por muestra está acotado:

- **Missile Scan Budget** (256 lecturas por consulta por defecto) limita cuántas lecturas DMA puede hacer una muestra; si se agota, el diagnóstico lo indica con `stage=budget` y no se dibuja nada de esa muestra.
- Con **Only Incoming Missiles** activo **no se recorre la consulta de bombas**, porque sus candidatos se descartan igualmente: la mitad de las lecturas.
- El intervalo es **adaptativo**: si una muestra tarda más de 15 ms, el intervalo se duplica hasta 1 s; si tarda menos de 4 ms, vuelve hacia 100 ms. En un enlace DMA lento el ESP se muestrea menos y deja de saturar el render.
- En listas grandes el recorrido es **incremental**: se procesan las filas que caben en el presupuesto y se guarda la posición exacta (sublista y fila); la muestra siguiente continúa desde ahí, sin perder filas ni bloquear el render. El diagnóstico marca `partial=1` cuando una muestra no cubrió toda la lista.
- Las **bengalas y señuelos** (`flare`, `smoke`, `chaff`, `countermeasure`, `pod`) se descartan por nombre **antes** de leer posición, velocidad y guiado, así que no consumen lecturas por fila.
- El misil se dibuja si **su posición** es visible: el marcador y el nombre no dependen de que el extremo de la trayectoria (`pos + vel * 0.5 s`) también lo sea. La línea se recorta al área de la pantalla.
- Si una muestra queda incompleta o falla una lectura, las trazas confirmadas se conservan hasta el límite de extrapolación; si la muestra fue completa y el misil ya no está, la traza se purga para no dejar fantasmas.

El coste real de cada muestra aparece en el diagnóstico como `ms=` y `reads=`, para poder ajustar el presupuesto sin recompilar.

El nombre se lee con `m_MissileNameCont` (`+0x720`) o `m_BombNameCont` (`+0x718`), la indirección de texto (`+0x50` en misiles, `+0x10` en bombas) y una lectura acotada de 63 bytes. La limpieza recibe el tamaño del búfer, corta en el primer byte no imprimible y exige al menos dos caracteres, así que un puntero erróneo no produce una etiqueta basura. Se normaliza para mostrar el nombre corto: se descarta `/short`, los directorios, el prefijo `carrier^` y la extensión `.blk`. Si no se puede resolver, el marcador se dibuja sin etiqueta. Los nombres se leen como máximo para 64 candidatos por consulta y solo si queda presupuesto de lectura; un fallo al leer el nombre nunca descarta al proyectil.

### Supuestos del perfil experimental

El recorrido se implementó a partir de un esquema externo y del JSON de offsets del usuario. Estos puntos son **supuestos**, no hechos verificados en juego:

- `g_EntityManager` es el propio manager (no un puntero a otro objeto).
- El selector `u32` se enmascara con `0xFFFFFF`; `0xFFFFFFFF` es inválido.
- Los registros de consulta miden `0x40` bytes y los descriptores `0x20`.
- Hasta 9 sublistas, los IDs van inline en `+0x04`; con más, se leen mediante el puntero de `+0x08`.
- La matriz de componentes es `u16` row-major; con más de 16 celdas se lee mediante el puntero de `+0x18`.
- Cada descriptor contiene **un único bloque** con `storage` en `+0x00`, `count` en `+0x08` y `shift` en `+0x0C`. El campo de `+0x10` no se interpreta.
- La componente 0 es la columna de punteros y la 1 la de estado `active` (byte `1` = activo).

Si la build real no coincide con estos supuestos, el diagnóstico lo señala y no se dibuja nada. Los offsets de campo del proyectil (`+0x2C0` posición, `+0x2DC` velocidad, `+0x58` propietario) provienen del JSON del usuario.

**Verificado en juego** (una sesión, build del usuario): selector `311` para cohetes y `318` para bombas; manager efectivamente inline; tablas `+0x178`/`+0x570` y registro `index + selector*0x40` correctos; descriptor con `storage` en `+0x00`, `count` en `+0x08` y `shift` en `+0x0C`; matriz de componentes en `+0x18` (`o0`/`o1`); nombre resuelto mediante `m_MissileNameCont + 0x50`. Los valores observados de posición y velocidad fueron coherentes con el mapa y con un misil aire-aire real, y el identificador de origen se mantuvo estable entre muestras. Esta comprobación corresponde a una build concreta: **no** garantiza otras versiones.

### Solo misiles que te apuntan (lock on me)

Con **Only Missiles Locked On Me** activado (viene activado por defecto) el ESP dibuja **únicamente los misiles que te están siguiendo** y descarta las bombas. No usa geometría ni ángulos, así que da igual cuánto gires o maniobres: un misil guiado hacia ti aparece aunque su vector de velocidad actual apunte a donde estabas.

- Del misil se lee `Projectile::m_MissileGuidance` (`+0x688`) y, dentro, `m_IsLocked` (`+0x4D`), `m_IsTracking` (`+0x80`) y `m_TargetUnitId` (`+0x84`, int16).
- Tu identificador se lee de `g_MyUnit` (puntero etiquetado: si el bit 0 está puesto se resta) y `Unit::m_UnitUid` (`+0x8`, uint16), una vez por muestra.
- Un misil pasa el filtro según la **sensibilidad** elegida:
  - `Strict lock`: `m_TargetUnitId == tu m_UnitUid`.
  - `Lock or entity id` (por defecto): también acepta `m_TargetUnitId == tus 16 bits bajos de m_EntityId`. Cubre el caso de que el campo del misil sea un *entity id* truncado a 16 bits y no el UID.
  - `Lock or nearby`: además acepta cualquier misil a menos de **Missile Nearby Distance** (3000 m) de ti, como red de seguridad cuando el guiado no identifica objetivo.
  - `All missiles`: solo excluye bombas.
- **Falla cerrada:** si no se puede leer lo que la sensibilidad necesita (menú, reaparición, lectura denegada), no se dibuja ningún proyectil y se avisa **una vez** por consola.
- El filtro no exige `m_IsLocked` ni `m_IsTracking`: un misil que conserva tu identificador pero ha perdido el lock sigue siendo una amenaza. Todos esos valores se registran en el diagnóstico.

**Pendiente de verificar en juego:** los offsets de guiado proceden del JSON del usuario pero no se han comprobado todavía contra la memoria real. El diagnóstico imprime tu `uid=` y `eid=`, y por candidato `locked=`, `tracking=` y `target=`, de modo que una sola pasada con un misil persiguiéndote confirma cuál de las dos referencias coincide.

### Diagnóstico y validación

Con **Projectile Debug** activo se emite una línea por tipo cada 5 segundos, más una línea inmediata cuando aparece el primer candidato de un tipo (transición de 0 a alguno), que es cuando las coordenadas importan:

```text
ECS EXPERIMENTAL kind=rocket BASE=0x... RVA=0x... selector=N record=0x... sublists=N visited=N rows=N active=N candidates=N rejected=N truncated=N reads=N stage=... req=N opt=N sub=N o0=N o1=N data=0x... index=0x... first=... names=N rawname="..." local=N uid=NNN filtered=N ms=N.N head=<64 hex> desc=<64 hex>
```

`stage` indica en qué punto se detuvo la lectura (`none`, `address`, `read`, `selector`, `layout`, `descriptor`, `budget`, `changed`, `skipped`). `skipped` es la línea de bombas cuando el filtro evita recorrerlas. `local` indica si se pudo leer tu identificador, `uid` cuál es y `filtered` cuántos candidatos descartó el filtro en la última muestra; por candidato, `first` incluye `locked=`, `tracking=` y `target=` para verificar el lock. `ms` es lo que tardó la última muestra y `reads` cuántas lecturas DMA hizo; `truncated` avisa de que se alcanzaron los 499 candidatos y quedaron filas sin visitar. `req`, `opt`, `sub`, `o0`, `o1`, `head` y `desc` muestran cómo se interpretó la cabecera del registro, los desplazamientos de componente y los bytes reales del registro y del primer descriptor. El decodificador revalida el selector, el registro (solo los 56 bytes que realmente consume), las tablas externas y cada descriptor antes de aceptar resultados, y revierte las filas asociadas si detecta cambios durante la lectura.

## Ejecutar y configurar

- Copia las DLL y demás dependencias de ejecución indicadas en [la guía DMA](WTC/lib/dma/README.md) junto al ejecutable.
- Se requiere el entorno DMA/dispositivo compatible; compilar correctamente no acredita funcionamiento con hardware ni compatibilidad con una versión actual del juego.
- **F1** abre/oculta el menú; **F2** cambia el preset. El README original especifica configuración de doble PC para esas teclas.
- La configuración se guarda en `%LOCALAPPDATA%\WTC\wtc_menu.cfg`, con alternativa junto al ejecutable si no se puede resolver la carpeta de usuario. No se incluyen perfiles personales.
- Los offsets predeterminados están en [el código](WTC/src/game/offsets.hpp). El menú permite crear `offsets.txt` junto al ejecutable y activar su lectura. No se distribuye la copia antigua de la carpeta de fuentes; actualiza los valores cuando corresponda.
- El renderizador usa Arial Bold instalada en `C:\Windows\Fonts\arialbd.ttf`. No se redistribuye esa fuente. La ubicación está fijada en el código; una instalación de Windows en otra ruta requiere adaptar [el renderizador](WTC/src/utils/render/render.cpp).
- El radar utiliza sus iconos vectoriales; no extrae ni procesa fuentes desde la memoria del juego. La apariencia puede diferir de los glifos originales.
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

La distribución excluye el SDK DMA de terceros, cachés del IDE, compilados, registros de compilación, preferencias personales, ejemplos de ImGui, diagnósticos de matrices X-ray y la carpeta de trabajo antigua de pruebas. Las pruebas de desarrollo y sus scripts de ejecución no se incluyen en esta distribución.

## Validación de esta distribución

Para esta publicación v4.1 se ejecutó localmente una suite de desarrollo (no incluida): **302 comprobaciones aprobadas**. Esta prueba no ejecuta el juego ni accede a hardware.

No se realizó una recompilación completa de la aplicación para esta publicación ni se verificó su funcionamiento en juego o con hardware DMA. Los resultados de compilaciones y suites de versiones anteriores no constituyen validación de esta entrega.

## Licencias y atribución

- Código del proyecto: [MIT, copyright 2026 Marazm](LICENSE), conservado del original.
- Dear ImGui: [licencia MIT y atribuciones](WTC/lib/imgui/LICENSE.txt). Se conservan también los avisos de sus componentes integrados.
- MemProcFS, LeechCore y FTDI: dependencias externas con condiciones propias, no cubiertas por la licencia MIT del proyecto. Consulta [la guía DMA](WTC/lib/dma/README.md) antes de redistribuir un programa enlazado o sus dependencias. Esta limpieza no certifica compatibilidad jurídica entre todas las licencias.
