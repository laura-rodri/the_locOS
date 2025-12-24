# the_locOS

Bienvenido a the_locOS, un sistema operativo tan “de locos” que hasta él mismo duda de su cordura.  
the_locOS es un lunático digital: un kernel inquieto, multihilo, un tanto impredecible, que no descansa mientras coordina procesos como si fuera un manicomio perfectamente organizado.

Solo los locos consiguen que todo funcione sin colgarse.

## Posibles Schedulers

Por defecto, the_locOS utiliza un scheduler Round Robin sin prioridades.  
    Se asume que el hecho de cambiar de contexto es instantáneo y no consume tiempo. Cuando un proceso termina su ejecución de forma natural, se elige inmediatamente el siguiente proceso a ejecutar. Cuando el quantum de un proceso expira, el scheduler vuelve a elegir un proceso. **(el scheduler no se ve afectado por los timers)**

El juego de la patata caliente: los procesos están sentados en un círculo, y cada uno tiene un quantum variable. Al de cierto tiempo, la patata explotará y matará al proceso que la tenga. El juego terminará cuando todos mueran o ya no queden más patatas.

Hundir la flota: si un proceso adivina correctamente una coordenada, puede seguir ejecutándose. Si falla, pierde su turno.

El ahorcado:

Brain fuck scheduler (BFS): originalmente creado por Con Kolivas, tiene una única cola global donde a cada tarea se le asigna un deadline. Para decidir qué tarea ejecutar a continuación, BFS escanea la lista de tareas en busca de la que tenga la fecha límite más cercana.

## Parte 3. Notas

mmu traduce direcciones lógicas a físicas, para eso tiene cache y una tlb en memoria. el registro PC apunta a la siguiente instrucción, el IR a la instrucción en ejecución; PCB tendrá algo parecido.  
memoria está partida en dos: kernel y usuario.

generar programas con prometheus. por cada uno hay que interpretar las líneas (según cantidad de datos) y ejecutar las instrucciones. 1ª pos: tipo de función, resto según la función (R = un registro, A = address)

heracles ayuda a interpretar los .elf. No hay que usarlo en el simulador, solo sirve para comprobar que se está decodificando correctamente.
