#!/bin/sh 

PID_SRV=""
PID_CLI=""
PID_TO=""

Matarile()
{
    # Matamos al ultimo cliente
    kill -9 $PID_CLI 2> /dev/null

    #Matamos al servidor
    kill -9 $PID_SRV 2> /dev/null

    #Matamos al sleep del timeout
    kill -9 $PID_TO  2> /dev/null
}

Alarma()
{
    echo "Timeout!!"
    Matarile

    exit 1
}

Timeout()
{
    # Esperamos los segundos indicados como argumento y nos
    # mandamos una se�al al propio script para capturarla 
    SECS=$1
    sleep $SECS && kill -14 $$ & 
    
    # $! es el PID del ultimo proceso en background 
    # Guardamos el PID del sleep que hace de timeout
    PID_TO=$!
}

trap Alarma 14

# La opcion -cli indica que se quieren trazar las llamadas de los clientes
if [ _$1 = "_-cli" ] 
    then
    # Quitamos el argumento 1 ($1) de la lista de argumentos de entrada
    shift 
    # Definimos dos variables para lanzar los clientes
    # Ejecutamos los clientes con un strace y eliminamos su salida estandar
    CLIENTE="strace ./cliente"
    CLIENTE_REDIRECT="/dev/null"
    SERVIDOR_REDIRECT="/dev/null"
else
    # Definimos dos variables para lanzar los clientes
    # Ejecutamos los clientes normal y no redirigimos las salidas
    CLIENTE="./cliente"
    CLIENTE_REDIRECT=""
    SERVIDOR_REDIRECT=""
fi

# La opcion -srv indica que se quieren trazar las llamadas del servidor
if [ _$1 = "_-srv" ] 
    then
    # Quitamos el argumento 1 ($1) de la lista de argumentos de entrada
    shift 
    # Ejecutamos el servidor con un strace eliminamos su salida estandar
    strace ./servidor > /dev/null &
    CLIENTE_REDIRECT="/dev/null"
else
    # Ejecutamos el servidor mandando su salida al error estandar
    if [ _$SERVIDOR_REDIRECT = _ ]
	then
	./servidor >&2 &
    else
	./servidor > $SERVIDOR_REDIRECT &
    fi
fi
# $! es el PID del ultimo proceso en background (el servidor)
PID_SRV=$!

sleep 1

# Recorremos todos los argumentos 
for i in $*
do
  # Pedimos que transfiera el fichero $i al cliente
  if [ _$CLIENTE_REDIRECT = _ ]
      then
      echo "LANZADOR: Transfiriendo el fichero $i"
      $CLIENTE $i &
  else
      $CLIENTE $i > $CLIENTE_REDIRECT &
  fi

  # $! es el PID del ultimo proceso en background (el cliente)
  PID_CLI=$!

  # Esperamos a que termine este cliente
  wait $PID_CLI

  # Comparamos los ficheros original y local
  diff $i "$i.local" > /dev/null 2>&1 || echo "LANZADOR: El fichero $i has sido transferido erroneamente" 

  rm -f "$i.local"

  sleep 1
done

if [ _$CLIENTE_REDIRECT = _ ]
then
    echo "LANZADOR: Pidiendo al servidor que termine (mensaje QUIT)" 
    $CLIENTE &
else
    $CLIENTE > $CLIENTE_REDIRECT &
fi

# $! es el PID del ultimo proceso en background (el cliente)
PID_CLI=$!

# Esperamos a que termine este cliente
wait $PID_CLI

echo "LANZADOR: Finalizado correctamente" 

Matarile
