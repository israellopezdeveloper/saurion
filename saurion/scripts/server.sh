#!/usr/bin/env node

const net = require('net');
const readline = require('readline');

// Función para convertir un buffer a una cadena hexadecimal
const bufferToHex = (buffer) => buffer.toString('hex');

// Crear la interfaz para leer desde el teclado
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

// Lista para almacenar los clientes conectados
let clients = [];

// Función para manejar los datos recibidos
const handleData = (socket, data) => {
  let offset = 0;

  while (offset < data.length) {
    // Verificar que hay suficientes bytes para leer la longitud
    if (data.length - offset < 8) {
      break;
    }

    // Leer los primeros 64 bits (8 bytes) como longitud
    const lengthBuffer = data.slice(offset, offset + 8);
    const length = Number(lengthBuffer.readBigUInt64LE());
    offset += 8;

    // Verificar que hay suficientes bytes para leer el mensaje completo
    if (data.length - offset < length) {
      break;
    }

    // Leer el mensaje basado en la longitud
    const messageBuffer = data.slice(offset, offset + length);
    offset += length;

    console.log(`${length}${messageBuffer}`);

    // Leer el resto del mensaje si hay
    const remainderBuffer = data.slice(offset);
    if (remainderBuffer.length > 0) {
    }
  }
};


// Crear el servidor
const server = net.createServer((socket) => {
  clients.push(socket);
  console.log(`connected`);

  socket.on('data', (data) => {
    handleData(socket, data);
  });

  socket.on('end', () => {
    clients = clients.filter(client => client !== socket);
    console.log(`disconnected`);
  });
});

server.listen(8080, () => {
});

// Leer entrada del usuario y enviarla a todos los clientes conectados
rl.on('line', (input) => {
  const length = Buffer.alloc(8);
  const message = Buffer.from(input);

  // Escribir la longitud del mensaje en los primeros 64 bits (8 bytes)
  length.writeBigUInt64LE(BigInt(message.length), 0);

  // Enviar la longitud y el mensaje concatenados a todos los clientes
  clients.forEach(client => {
    client.write(Buffer.concat([length, message]));
  });

});

