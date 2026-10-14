# Dependencias DMA externas

Los archivos del SDK y los binarios de terceros no se redistribuyen en este paquete. Debes obtenerlos por separado de sus autores y cumplir sus licencias. No descargues DLL sueltas desde sitios no oficiales.

## Para compilar (x64)

Coloca en esta carpeta un conjunto compatible de:

| Archivo | Proyecto/proveedor |
| --- | --- |
| `vmmdll.h` y `vmm.lib` | [MemProcFS](https://github.com/ufrisk/MemProcFS) |
| `leechcore.h` y `leechcore.lib` | [LeechCore](https://github.com/ufrisk/LeechCore) |
| `FTD3XX.lib` | [FTDI D3XX](https://ftdichip.com/drivers/d3xx-drivers/) |

Los encabezados del proyecto original declaran versiones **MemProcFS 5.8** y **LeechCore 2.9.1**. La versión exacta de las bibliotecas de importación originales no está identificada. Usa headers, bibliotecas de importación y DLL de versiones compatibles, todos x64; no se garantiza compatibilidad con la última versión disponible. Consulta las distribuciones SDK o las instrucciones de compilación de los autores para obtener los `.lib`.

## Conservar la compilación local al preparar una publicación

Excluir el SDK del paquete público no significa eliminarlo del proyecto de trabajo. Mantén estos cinco archivos en la copia local y prepara la distribución en otra carpeta. Las DLL por sí solas no sustituyen los encabezados ni las bibliotecas de importación; tampoco sirven encabezados ficticios o archivos `.lib` vacíos.

Si aparece C1083 al incluir `vmmdll.h` desde `src/memory/memory.h`, comprueba que los cinco archivos anteriores existan aquí. La ruta relativa del include es correcta. Restaura el conjunto de desarrollo de tu copia funcional, sin sobrescribir el resto del código.

## Para ejecutar

Coloca `vmm.dll`, `leechcore.dll` y `FTD3XX.dll` junto al ejecutable generado en `WTC/build/x64/Release/`. Esos archivos no son necesarios para enlazar cuando las bibliotecas de importación ya están disponibles, pero sí para ejecutar. La compilación no los descarga ni copia automáticamente.

MemProcFS puede requerir archivos y plugins adicionales según dispositivo y funciones utilizadas. El código original documenta también `symsrv.dll`, `dbghelp.dll` e `info.db` para su ruta FixCR3. Sigue las instrucciones de la distribución elegida; no se ha comprobado aquí la ejecución ni la lista transitiva completa.

## Licencias

La licencia MIT del código propio no concede derechos sobre estas dependencias. El README original identifica MemProcFS como AGPL-3.0. Comprueba las condiciones exactas de las versiones de MemProcFS, LeechCore y FTDI que utilices y sus efectos sobre la distribución del programa combinado. Excluir los binarios de este repositorio no resuelve por sí solo esas obligaciones.
