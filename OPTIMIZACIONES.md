# Mejoras de predicción y coste por frame

## Implementado

- **G2G:** modelo independiente en [ground_prediction.hpp](WTC/src/features/aimbot/ground_prediction.hpp). Giro horizontal con signo, aceleración longitudinal filtrada, frenada sin invertir velocidad, horizonte de maniobra limitado a 0,6 s y alineación de posición del snapshot con velocidad actual. La aceleración vertical de baches no se extrapola.
- **Balística:** resistencia disipativa, integración simétrica de gravedad/resistencia con coeficiente Mach de punto medio y posiciones relativas al origen. Misma resolución para blancos quietos y móviles; convergencia según error de impacto, con salida válida explícita. Calibre de tablas convertido de mm a metros antes del modelo terrestre; se conserva la convención en metros del fallback global.
- **Presupuesto G2G:** máximo 10 iteraciones de 512 pasos, sin asignaciones dinámicas en el solver. Antes: hasta aproximadamente 48.000 pasos; ahora: 5.120. Una sola caché para el objetivo seleccionado. Lecturas y cálculo cada 60 ms en movimiento / 120 ms parado; el presupuesto sobrevive a cambios de objetivo. Dibujo y suavizado temporal siguen por frame.
- **Aim lead aéreo:** lecturas de velocidad/aceleración, filtrado y solve comparten el intervalo de 30 ms. Los fallos también se limitan. Posición local del arma compartida entre objetivos, refrescada cada 100 ms y al cambiar unidad local. No se ha sustituido la física A2A.
- **X-ray:** se conservan las capacidades de los vectores de relleno entre frames. El anterior clear del mapa destruía esos vectores. Claves por categoría (máximo siete) evitan crecimiento al cambiar color/alpha/distancia. Compactación de triángulos in-place y eliminación de búsquedas redundantes en nombres. No se cachean proyecciones de cámara ni se congelan transforms animados.
- **Misiles:** índice reutilizable para búsqueda de tracks anteriores, reconstruido por muestra; coseno del filtro entrante una vez por muestra. Esa búsqueda pasa de O(candidatos × tracks) a O(tracks log tracks + candidatos log tracks). La deduplicación restante no se ha cambiado. Protocolo ECS, filtros, TTL y presupuestos DMA se conservan.

## Verificación

- Compilación Release x64 con MSVC v145; registro en [optimization-build.log](WTC/build/optimization-build.log).
- 302 comprobaciones nativas ejecutadas localmente: frenadas, signo de giro, simetría, desfase temporal, datos inválidos, alcance, traslación a coordenadas grandes y límite de trabajo.
- Comparación contra integración RK4 independiente de paso ≤1 ms: peor desviación observada **0,1673 m** en los casos aceptados del conjunto probado. Esto verifica coherencia numérica del modelo, no precisión real en el juego.
- Barrido adicional: 33 casos aceptados y 3 rechazados dentro del presupuesto; máximo observado 1.350 pasos por cálculo.
- Microbenchmark de 10.000 solves: aproximadamente **6,8 µs por solve** en esta máquina, sin DMA ni render. No es una medida de FPS ni una comparación de rendimiento en partida.
- Revisión de equivalencia de los cambios de misiles y compactación X-ray; compilación nativa de su integración.

Las pruebas de desarrollo y sus scripts no se distribuyen en este repositorio.

## Límites y comprobación en partida

- Horizonte balístico G2G: **8 s**. Muestras con más de **250 ms** se descartan. Sin convergencia no se presenta un marcador aparentemente válido.
- La maniobra futura es una estimación conservadora, no conocimiento del próximo movimiento. Los límites terrestres necesitan calibración con datos reales. Se conserva la altura base histórica del marcador.
- No se han medido FPS, latencia DMA real ni precisión con proyectiles del juego. Tampoco se ha reescrito indiscriminadamente todo el proyecto.
- Comparar en la misma escena/configuración: recta, frenada, curva a ambos lados, cambio rápido de objetivo; después activar X-ray relleno y wireframe, varios indicadores aéreos y misiles. Observar frametime p95/p99 además de FPS medios y comprobar que no quedan marcadores tras desaparecer/cambiar el objetivo.

Fuentes principales: [integración G2G](WTC/src/features/aimbot/aimbot.hpp), [ESP y caches](WTC/src/features/esp/esp.hpp), [misiles](WTC/src/features/esp/projectile_esp.hpp), [geometría X-ray](WTC/src/features/esp/xray_bvh.hpp).
