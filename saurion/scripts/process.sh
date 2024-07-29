#!/bin/bash

rm thread_*.log

# Comprobar que se ha pasado el archivo como argumento
input_file="errors.txt"

# Verificar si el archivo de entrada existe
if [ ! -f "$input_file" ]; then
	echo "El archivo $input_file no existe."
	exit 1
fi

# Procesar el archivo y dividir los logs por threads
while IFS= read -r line; do
	# Extraer el identificador del thread y el log
	thread_id=$(echo "$line" | grep -oP '(?<=<)\d+(?=>)')
	log_message=$(echo "$line" | sed -e 's/<[0-9]\+>//')

	# Escribir el log en el archivo correspondiente al thread
	echo "$log_message" >>"thread_${thread_id}.log"
done <"$input_file"

echo "Logs divididos por thread en archivos separados."
