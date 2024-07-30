#!/usr/bin/env node

const net = require('net');
const readline = require('readline');
const fs = require('fs');
const os = require('os');
const path = require('path');

// Variables globales
let clients = [];
let numClients = 0;

// Crear la interfaz para leer desde el teclado
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

// Parsear los argumentos de la línea de comandos
const args = process.argv.slice(2);
let pipePath = '';

for (let i = 0; i < args.length; i++) {
  if (args[i] === '-p' && i + 1 < args.length) {
    pipePath = args[i + 1];
    break;
  }
}

if (!pipePath) {
  process.exit(1);
}

const tempFile2 = path.join(os.tmpdir(), `saurion_temp.log`);
const logStream2 = fs.createWriteStream(tempFile2, { flags: 'a' });

const createClient = (clientId) => {
  // Crear archivo temporal para el cliente
  const tempFile = path.join(os.tmpdir(), `saurion_sender.${clientId}.log`);
  const logStream = fs.createWriteStream(tempFile, { flags: 'a' });
  logStream.on('error', (err) => {
    console.error(`Error en el cliente ${clientId}:`, err.message);
  });

  // Crear una conexión al servidor
  const client = net.createConnection({ port: 8080 }, () => {
  });

  let partialMessage = '';
  let expectedLength = null;

  client.on('data', (data) => {
    let offset = 0;

    // Si no se ha leído la longitud del mensaje, leer los primeros 64 bits (8 bytes) como longitud
    if (expectedLength === null) {
      const lengthBuffer = data.slice(offset, offset + 8);
      expectedLength = Number(lengthBuffer.readBigUInt64LE());
      offset += 8;
    }
    logStream.write(data.slice(offset).toString());
    offset = 0;
  });

  client.on('end', () => {
    logStream.end();
  });

  client.on('error', (err) => {
    console.error(`Error en el cliente ${clientId}:`, err.message);
  });

  return client;
};

const connectClients = (n) => {
  for (let i = 0; i < n; i++) {
    const client = createClient(numClients + i + 1);
    clients.push(client);
  }
  numClients += n;
};

const sendMessages = (n, msg, delay) => {
  msg = (msg === "(null)" ? "" : msg)
  const length = Buffer.alloc(8);
  const message = Buffer.from(msg + '\0');
  length.writeBigUInt64LE(BigInt(message.length - 1), 0);
  // logStream2.write(`n->${n}[${length}] - msg->${message} - delay->${delay}`);
  clients.forEach((client, index) => {
    for (let i = 0; i < n; i++) {
      setTimeout(() => {
        client.write(Buffer.concat([length, message]));
      }, i * delay);
    }
  });
};

const disconnectClients = () => {
  clients.forEach((client) => {
    client.end();
  });
  clients = [];
  numClients = 0;
};

const closeApplication = () => {
  logStream2.close();
  fs.unlinkSync(tempFile2);
  disconnectClients();
  try {
    rl.close();
  } catch (e) { }
  process.exit(0);
};

process.on('SIGINT', () => {closeApplication();});

let buffer = '';

const handleCommand = (command) => {
  const parts = command.split(';');
  const cmd = parts[0];
  switch (cmd) {
    case 'connect':
      const n = parseInt(parts[1], 10);
      console.log(`==>${n}`);
      connectClients(n);
      break;
    case 'send':
      const count = parseInt(parts[1], 10);
      const msg = parts[2];
      const delay = parseInt(parts[3], 10);
      sendMessages(count, msg, delay);
      break;
    case 'disconnect':
      disconnectClients();
      break;
    case 'close':
      closeApplication();
      break;
    default:
      break;
  }
};

const processBuffer = () => {
  let endIndex;
  
  while ((endIndex = buffer.indexOf('\n')) !== -1) {
    const command = buffer.slice(0, endIndex);
    handleCommand(command.trim());
    buffer = buffer.slice(endIndex + 1);
  }
  
  // Procesar cualquier comando restante que no esté seguido de '\n'
  if (buffer.length > 0) {
    handleCommand(buffer.trim());
    buffer = '';
  }
};

const readPipe = () => {
  fs.open(pipePath, 'r', (err, fd) => {
    if (err) {
      console.error('Error abriendo el pipe:', err.message);
      setTimeout(readPipe, 1000); // Reintentar después de 1 segundo
      return;
    }

    const readChunk = Buffer.alloc(4096); // Tamaño de lectura más pequeño

    const readFromPipe = () => {
      fs.read(fd, readChunk, 0, readChunk.length, null, (err, bytesRead) => {
        if (err) {
          console.error('Error leyendo del pipe:', err.message);
          fs.close(fd, () => { }); // Cerrar el descriptor de archivo
          setTimeout(readPipe, 1000); // Reintentar después de 1 segundo
          return;
        }
        if (bytesRead > 0) {
          buffer += readChunk.toString('utf8', 0, bytesRead);
          if (buffer.includes('\n') || bytesRead < readChunk.length) {
            processBuffer();
          }
          readFromPipe();
        } else {
          // EOF o pipe cerrado
          if (buffer.length > 0) {
            processBuffer(); // Procesar cualquier dato restante
          }
          fs.close(fd, () => { });
          setTimeout(readPipe, 1000); // Reintentar después de 1 segundo
        }
      });
    };

    readFromPipe();
  });
};
readPipe();
