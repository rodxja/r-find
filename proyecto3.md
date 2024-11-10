# Tecnológico de Costa Rica
# Escuela de Computación
# Curso: Principios de Sistemas Operativos
# Tercer Proyecto

El comando find de Unix permite buscar y encontrar archivos en el sistema
de directorios con base en distintos criterios. En este proyecto se pretende
programar un aplicación que funcionará en forma similar a dicho comando pero
actuando sobre un servidor remoto.
La idea es crear dos programas, el programa cliente rfind que podrá ser ejecutado
en cualquier computador; y el programa rfind_server que se ejecutará en una
máquina especifica de la red. Inicialmente se debe ejecutar rfind_server en un
directorio particular del computador servidor mediante un número de socket
asignado al servicio (p.ej. 8899). Luego se debe ejecutar en la línea de comando
de Unix el programa cliente rfind que solicitará buscar en el servidor algún o
algunos archivos. Dicho programa cliente contará con una opción para transferir,
a la máquina cliente, todos los archivos especificados mediante el criterio de
búsqueda.

## Uso del programa
El programa rfind recibe una expresión regular que representa los archivos a
buscar:
```
rfind . -name SEARCH_NAME
```
En este caso el punto (.) indica que se debe buscar a partir de la raíz del servidor,
"-name" indica que se buscará por el nombre de archivo, y SEARCH_NAME
representa la expresión regular (entre comillas) para especificar los nombres de
los archivos a buscar.
El resultado del comando será un listado, mostrado en la máquina cliente, de
cada uno de los archivos en el servidor que cumplen con dicha condición de
búsqueda.
Una opción adicional (get) permitirá recuperar en la máquina cliente todos los
archivos que cumplen con la condicion de búsqueda, por ejemplo:
```
rfind ./source -get -name "*.cpp"
```
Este comando buscará y recuperará desde el directorio "source" del servidor
todos los archivos que terminen con la extensión "cpp".

### Manejo de expresiones regulares
El lenguaje C por sí mismo no maneja expresiones regulares, sin embargo se
puede usar la librería "regex.h" que cuenta con las funciones regcomp y regexec que
permiten realizar esta tarea. La función regcomp se debe ejecutar inicialmente y
recibe la expresión regular ingresada por comando, luego la función regexec se
debe ejecutar sobre cada una de los nombres de los archivos en el servidor.
## Documentación
Se deberá generar una documentación formal, en formato pdf, en donde se
describan las diferentes etapas del desarrollo del proyecto, las decisiones de diseño
que se tomaron, los mecanismos de programación utilizados, y los resultados de
las diferentes pruebas al programa.
Dicha documentación deberá incluir al menos las siguientes secciones:
- Introducción
- Descripción del problema (este enunciado)
- Definición de estructuras de datos
- Descripción detallada y explicación de los componentes principales del
servidor/cliente:
- Manejo de sockets
- Manejo de solicitudes
- Rutina de transferencia
- Descripción del protocolos y formatos utilizados
- Análisis de resultados de pruebas
- Conclusiones sobre rendimiento y correctitud
## Consideraciones generales
- El proyecto debe ser ejecutado en grupos de a lo más dos personas. Se
puede desarrollar en forma individual pero bajo ninguna circunstancia se
aceptarán grupos de tres o más personas.
- El proyecto debe ser realizado en lenguaje C
- No se permite la copia de código entre grupos de estudiantes, tampoco es
permitido utilizar clases o librerías adicionales (desarrolladas por terceros)
para simplificar el manejo de socket, autenticación, o copia de archivos.
- Se deberá crear un archivo zip que incluya: el código fuente de todos los
programas desarrollados y documentación en formato pdf.
2
- Puede buscar en Internet mayor información sobre el uso del comando find,
pero note que no todas las opciones de dicha aplicación serán implementadas
en este proyecto.