const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
const os = require('os');
const { spawn } = require('child_process');
const url = require('url');
const WebSocket = require('ws');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: '*',
  }
});

const PORT = process.env.PORT || 3000;
let tunnelUrl = null;
let tunnelProcess = null;
let unrealSocket = null;

function sendToUnreal(payload) {
  if (unrealSocket && unrealSocket.readyState === WebSocket.OPEN) {
    unrealSocket.send(JSON.stringify(payload));
  }
}

function canAcceptPlayerInput(playerId) {
  if (gameState === 'Playing') return true;
  return gameState === 'Result' && winner && winner.playerId === playerId;
}

function getLocalIpAddress() {
  const interfaces = os.networkInterfaces();
  for (const devName in interfaces) {
    const iface = interfaces[devName];
    for (let i = 0; i < iface.length; i++) {
      const alias = iface[i];
      if (alias.family === 'IPv4' && alias.address !== '127.0.0.1' && !alias.internal) {
        return alias.address;
      }
    }
  }
  return '127.0.0.1';
}

// Middleware
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// In-Memory Database
let gameState = 'Lobby'; // 'Lobby' | 'Shop' | 'Countdown' | 'Playing' | 'Result'
let players = {}; // playerId -> player details
let ranking = []; // List of players who died (from first to die to last)
let winner = null;

// Shop Items Configuration
const INITIAL_SHOP_ITEMS = {
  atk_boost: { itemId: 'atk_boost', name: '공격 강화', stock: 10, effect: { type: 'AttackPower', value: 1.2 } },
  speed_boost: { itemId: 'speed_boost', name: '이동속도 증가', stock: 10, effect: { type: 'Speed', value: 1.2 } },
  range_boost: { itemId: 'range_boost', name: '공격범위 증가', stock: 5, effect: { type: 'Range', value: 1.3 } },
  shield: { itemId: 'shield', name: '시작 보호막', stock: 5, effect: { type: 'Shield', value: 50 } }
};
let shopItems = JSON.parse(JSON.stringify(INITIAL_SHOP_ITEMS));

// Spectator Votes & Cheers
let voteCounts = { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
let cheerCounts = {}; // targetPlayerId -> count

const fs = require('fs');

// Save game result helper
function saveGameResult(winner, ranking) {
  const historyPath = path.join(__dirname, 'history.json');
  let history = [];
  try {
    if (fs.existsSync(historyPath)) {
      history = JSON.parse(fs.readFileSync(historyPath, 'utf8'));
    }
  } catch (e) {
    console.error('Failed to read history:', e);
  }

  const newResult = {
    gameId: 'g_' + Date.now(),
    timestamp: Date.now(),
    winner: winner ? { playerId: winner.playerId, nickname: winner.nickname } : null,
    ranking: ranking,
    playerCount: Object.keys(players).length
  };

  history.push(newResult);

  if (history.length > 50) {
    history = history.slice(history.length - 50);
  }

  try {
    fs.writeFileSync(historyPath, JSON.stringify(history, null, 2), 'utf8');
  } catch (e) {
    console.error('Failed to save history:', e);
  }
}

// Server Side Physics / Position Simulation Loop
let physicsInterval = null;
const SERVER_MOVE_SPEED = 0.8;
const UNREAL_MAP_HALF_SIZE = 3000;
const UNREAL_WORLD_STATE_TIMEOUT_MS = 1500;
let lastUnrealWorldStateAt = 0;

function clampPercent(value) {
  return Math.max(0, Math.min(100, value));
}

function worldToPercent(worldX, worldY) {
  return {
    posX: clampPercent(((worldY + UNREAL_MAP_HALF_SIZE) / (UNREAL_MAP_HALF_SIZE * 2)) * 100),
    posY: clampPercent(100 - (((worldX + UNREAL_MAP_HALF_SIZE) / (UNREAL_MAP_HALF_SIZE * 2)) * 100))
  };
}

function hasRecentUnrealWorldState() {
  return Date.now() - lastUnrealWorldStateAt <= UNREAL_WORLD_STATE_TIMEOUT_MS;
}

function buildCompactPositions() {
  const compactPositions = {};
  Object.keys(players).forEach(id => {
    compactPositions[id] = [
      Math.round((players[id].posX ?? 50) * 10) / 10,
      Math.round((players[id].posY ?? 50) * 10) / 10,
      players[id].state === 'Alive' ? 1 : 0,
      Math.round((players[id].hp ?? 100) * 10) / 10,
      Math.round((players[id].maxHp ?? 100) * 10) / 10,
      Number.isFinite(players[id].worldX) ? Math.round(players[id].worldX * 10) / 10 : null,
      Number.isFinite(players[id].worldY) ? Math.round(players[id].worldY * 10) / 10 : null
    ];
  });
  return compactPositions;
}
function startServerPhysicsLoop() {
  if (physicsInterval) clearInterval(physicsInterval);
  
  // Initialize positions
  Object.keys(players).forEach(id => {
    players[id].posX = 15 + Math.random() * 70;
    players[id].posY = 15 + Math.random() * 70;
    players[id].x = 0;
    players[id].y = 0;
  });
  
  physicsInterval = setInterval(() => {
    const inputsToSend = [];

    Object.keys(players).forEach(id => {
      const p = players[id];
      if (!p) return;
      
      if (p.state !== 'Alive') return;
      
      // Simulate bots locally
      if (p.isBot) {
        if (p.x === undefined) p.x = 0;
        if (p.y === undefined) p.y = 0;
        
        if (Math.random() < 0.05) {
          const angle = Math.random() * Math.PI * 2;
          const isMoving = Math.random() < 0.7;
          p.x = isMoving ? Math.cos(angle) : 0;
          p.y = isMoving ? Math.sin(angle) : 0;
        }
      }

      inputsToSend.push({
        playerId: id,
        moveX: p.x,
        moveY: p.y
      });
      
      if (!hasRecentUnrealWorldState() && (p.x !== 0 || p.y !== 0)) {
        p.posX = (p.posX || 50) + p.x * SERVER_MOVE_SPEED;
        p.posY = (p.posY || 50) - p.y * SERVER_MOVE_SPEED;
        
        p.posX = Math.max(2, Math.min(98, p.posX));
        p.posY = Math.max(2, Math.min(98, p.posY));
      }
    });
    
    // Broadcast positions to all clients
    io.emit('positionsUpdated', buildCompactPositions());

    // Push bot/player inputs to Unreal via WebSocket
    sendToUnreal({
      type: 'inputsUpdated',
      inputs: inputsToSend
    });
  }, 100);
}

function stopServerPhysicsLoop() {
  if (physicsInterval) {
    clearInterval(physicsInterval);
    physicsInterval = null;
  }
}

function markPlayerDead(playerId) {
  if (!playerId || !players[playerId]) return false;

  const oldState = players[playerId].state;
  players[playerId].state = 'Dead';
  players[playerId].hp = 0;

  if (oldState !== 'Dead') {
    ranking.push({
      playerId,
      nickname: players[playerId].nickname,
      timeOfDeath: Date.now()
    });

    io.emit('playerDead', { playerId, nickname: players[playerId].nickname });
    checkGameEndingCondition();
    return true;
  }

  return false;
}

// Helper to broadcast state to all clients
function broadcastState() {
  const statePayload = {
    gameState,
    players: Object.values(players),
    ranking,
    winner,
    shopItems,
    voteCounts,
    cheerCounts
  };
  io.emit('gameStateChanged', statePayload);
  
  sendToUnreal({
    type: 'gameStateChanged',
    ...statePayload
  });
}

// Generate unique player ID
function generatePlayerId() {
  return 'p_' + Math.random().toString(36).substr(2, 9);
}

// --- REST API Endpoints ---

// Get server host info (local IP, tunnel URL, etc.)
app.get('/api/host-info', (req, res) => {
  const localIp = getLocalIpAddress();
  res.json({
    localIp,
    port: PORT,
    tunnelUrl: tunnelUrl
  });
});

// Get current game status
app.get('/api/status', (req, res) => {
  res.json({
    gameState,
    playerCount: Object.keys(players).length,
    players: Object.values(players),
    ranking,
    winner,
    shopItems,
    voteCounts,
    cheerCounts
  });
});

// Join game
app.post('/api/join', (req, res) => {
  const { nickname, color } = req.body;
  if (!nickname || nickname.trim() === '') {
    return res.status(400).json({ success: false, reason: 'Nickname is required' });
  }

  // If game is already running, join as Spectator
  const playerState = (gameState === 'Lobby') ? 'Joined' : 'Spectator';
  const playerId = generatePlayerId();

  players[playerId] = {
    playerId,
    nickname: nickname.trim().substring(0, 8),
    color: color || '#ff4d4d',
    state: playerState,
    x: 0,
    y: 0,
    posX: 15 + Math.random() * 70,
    posY: 15 + Math.random() * 70,
    hp: 100,
    maxHp: 100,
    connected: true,
    joinedAt: Date.now(),
    items: [],
    cheerTargetId: null,
    vote: null
  };

  broadcastState();

  res.json({
    success: true,
    playerId,
    nickname: players[playerId].nickname,
    state: playerState
  });
});

// Send input (alternative HTTP API)
app.post('/api/input', (req, res) => {
  const { playerId, moveX, moveY } = req.body;
  if (!playerId || !players[playerId]) {
    return res.status(404).json({ success: false, reason: 'Player not found' });
  }

  if (!canAcceptPlayerInput(playerId)) {
    return res.status(400).json({ success: false, reason: 'Player input is not accepted in current game state' });
  }

  players[playerId].x = Number(moveX) || 0;
  players[playerId].y = Number(moveY) || 0;

  // Broadcast inputs to host/unreal
  io.emit('inputsUpdated', {
    playerId,
    moveX: players[playerId].x,
    moveY: players[playerId].y
  });

  sendToUnreal({
    type: 'inputsUpdated',
    inputs: [
      {
        playerId,
        moveX: players[playerId].x,
        moveY: players[playerId].y
      }
    ]
  });

  res.json({ success: true });
});

// Get shop items
app.get('/api/shop', (req, res) => {
  res.json({
    success: true,
    items: Object.values(shopItems)
  });
});

// Buy item
app.post('/api/buy', (req, res) => {
  const { playerId, itemId } = req.body;
  if (gameState !== 'Shop') {
    return res.status(400).json({ success: false, reason: 'NotShopState' });
  }
  if (!playerId || !players[playerId]) {
    return res.status(404).json({ success: false, reason: 'PlayerNotFound' });
  }
  if (!itemId || !shopItems[itemId]) {
    return res.status(404).json({ success: false, reason: 'ItemNotFound' });
  }
  if (shopItems[itemId].stock <= 0) {
    return res.status(400).json({ success: false, reason: 'SoldOut' });
  }

  shopItems[itemId].stock -= 1;
  if (!players[playerId].items) players[playerId].items = [];
  players[playerId].items.push(itemId);

  broadcastState();
  res.json({
    success: true,
    items: players[playerId].items
  });
});

// Post spectator vote
app.post('/api/vote', (req, res) => {
  const { playerId, eventType } = req.body;
  if (gameState !== 'Playing') {
    return res.status(400).json({ success: false, reason: 'GameNotPlaying' });
  }
  if (!playerId || !players[playerId]) {
    return res.status(404).json({ success: false, reason: 'PlayerNotFound' });
  }
  const p = players[playerId];
  if (p.state !== 'Dead' && p.state !== 'Spectator') {
    return res.status(400).json({ success: false, reason: 'PlayerMustBeDeadToVote' });
  }

  p.vote = eventType;

  // Recalculate vote counts
  voteCounts = { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
  Object.values(players).forEach(pl => {
    if ((pl.state === 'Dead' || pl.state === 'Spectator') && pl.vote) {
      if (voteCounts[pl.vote] !== undefined) {
        voteCounts[pl.vote]++;
      }
    }
  });

  broadcastState();
  res.json({ success: true, voteCounts });
});

// Post spectator cheer
app.post('/api/cheer', (req, res) => {
  const { playerId, targetId } = req.body;
  if (gameState !== 'Playing') {
    return res.status(400).json({ success: false, reason: 'GameNotPlaying' });
  }
  if (!playerId || !players[playerId]) {
    return res.status(404).json({ success: false, reason: 'PlayerNotFound' });
  }
  if (!targetId || !players[targetId]) {
    return res.status(404).json({ success: false, reason: 'TargetPlayerNotFound' });
  }

  const p = players[playerId];
  if (p.state !== 'Dead' && p.state !== 'Spectator') {
    return res.status(400).json({ success: false, reason: 'PlayerMustBeDeadToCheer' });
  }
  const target = players[targetId];
  if (target.state !== 'Alive') {
    return res.status(400).json({ success: false, reason: 'TargetMustBeAlive' });
  }

  p.cheerTargetId = targetId;

  // Recalculate cheer counts
  cheerCounts = {};
  Object.values(players).forEach(pl => {
    if ((pl.state === 'Dead' || pl.state === 'Spectator') && pl.cheerTargetId) {
      cheerCounts[pl.cheerTargetId] = (cheerCounts[pl.cheerTargetId] || 0) + 1;
    }
  });

  broadcastState();
  res.json({ success: true, cheerCounts });
});

// Get history
app.get('/api/history', (req, res) => {
  const historyPath = path.join(__dirname, 'history.json');
  let history = [];
  try {
    if (fs.existsSync(historyPath)) {
      history = JSON.parse(fs.readFileSync(historyPath, 'utf8'));
    }
  } catch (e) {
    console.error('Failed to read history:', e);
  }
  res.json(history);
});

// Unreal client gets all players
app.get('/api/unreal/players', (req, res) => {
  res.json({
    players: Object.values(players)
  });
});

// Unreal client gets all player inputs
app.get('/api/unreal/inputs', (req, res) => {
  const inputs = Object.values(players).map(p => ({
    playerId: p.playerId,
    moveX: p.x,
    moveY: p.y,
    timestamp: Date.now()
  }));
  res.json({ inputs });
});

// Unreal reports authoritative world positions and HP.
app.post('/api/unreal/world-state', (req, res) => {
  const reportedPlayers = Array.isArray(req.body.players) ? req.body.players : [];
  lastUnrealWorldStateAt = Date.now();
  let shouldBroadcastState = false;

  reportedPlayers.forEach(snapshot => {
    const playerId = snapshot && snapshot.playerId;
    if (!playerId || !players[playerId]) return;

    const worldX = Number(snapshot.worldX);
    const worldY = Number(snapshot.worldY);
    if (Number.isFinite(worldX) && Number.isFinite(worldY)) {
      const percentPosition = worldToPercent(worldX, worldY);
      players[playerId].worldX = worldX;
      players[playerId].worldY = worldY;
      players[playerId].posX = percentPosition.posX;
      players[playerId].posY = percentPosition.posY;
    }

    const hp = Number(snapshot.hp);
    const maxHp = Number(snapshot.maxHp);
    if (Number.isFinite(hp)) {
      players[playerId].hp = Math.max(0, hp);
    }
    if (Number.isFinite(maxHp) && maxHp > 0) {
      players[playerId].maxHp = maxHp;
    }

    if (snapshot.alive === false || players[playerId].hp <= 0) {
      shouldBroadcastState = markPlayerDead(playerId) || shouldBroadcastState;
    } else if (players[playerId].state !== 'Dead' && players[playerId].state !== 'Winner') {
      shouldBroadcastState = players[playerId].state !== 'Alive' || shouldBroadcastState;
      players[playerId].state = 'Alive';
    }
  });

  io.emit('positionsUpdated', buildCompactPositions());
  if (shouldBroadcastState) {
    broadcastState();
  }
  res.json({ success: true, count: reportedPlayers.length });
});

// Unreal updates player state (e.g. Dead)
app.post('/api/unreal/player-state', (req, res) => {
  const { playerId, state } = req.body;
  if (!playerId || !players[playerId]) {
    return res.status(404).json({ success: false, reason: 'Player not found' });
  }

  if (state === 'Dead') {
    markPlayerDead(playerId);
  } else {
    players[playerId].state = state;
  }

  broadcastState();
  res.json({ success: true });
});

// Unreal posts game results
app.post('/api/unreal/result', (req, res) => {
  const { winnerId, rankingList } = req.body;
  
  if (winnerId && players[winnerId]) {
    winner = players[winnerId];
    players[winnerId].state = 'Winner';
  }
  
  if (rankingList && Array.isArray(rankingList)) {
    ranking = rankingList;
  }

  gameState = 'Result';
  stopServerPhysicsLoop();

  // Save result to history.json
  saveGameResult(winner, ranking);

  broadcastState();
  res.json({ success: true });
});

// Admin command: Start Shop
app.post('/api/admin/start-shop', (req, res) => {
  if (gameState !== 'Lobby') {
    return res.status(400).json({ success: false, reason: 'Game must be in Lobby state' });
  }

  gameState = 'Shop';
  shopItems = JSON.parse(JSON.stringify(INITIAL_SHOP_ITEMS));
  
  // Reset player items, vote, cheer
  Object.keys(players).forEach(id => {
    players[id].items = [];
    players[id].cheerTargetId = null;
    players[id].vote = null;
  });

  broadcastState();
  res.json({ success: true, gameState });
});

// Admin command: Start Game
app.post('/api/admin/start-game', (req, res) => {
  if (gameState !== 'Lobby' && gameState !== 'Shop') {
    return res.status(400).json({ success: false, reason: 'Game must be in Lobby or Shop state' });
  }

  gameState = 'Playing';
  ranking = [];
  winner = null;

  // Set all joined players to Alive
  Object.keys(players).forEach(id => {
    if (players[id].state === 'Joined' || players[id].state === 'Ready') {
      players[id].state = 'Alive';
    } else {
      players[id].state = 'Spectator';
    }
    players[id].x = 0;
    players[id].y = 0;
    players[id].hp = 100;
    players[id].maxHp = players[id].maxHp || 100;
  });

  broadcastState();
  startServerPhysicsLoop();
  res.json({ success: true, gameState });
});

// Admin command: Reset Game
app.post('/api/admin/reset', (req, res) => {
  gameState = 'Lobby';
  ranking = [];
  winner = null;

  // Reset shop items, votes, cheers
  shopItems = JSON.parse(JSON.stringify(INITIAL_SHOP_ITEMS));
  voteCounts = { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
  cheerCounts = {};

  // Reset player states, keep bots if desired, but remove bots or reset them
  Object.keys(players).forEach(id => {
    if (players[id].isBot) {
      delete players[id]; // clean up bots on reset
    } else {
      players[id].state = 'Joined';
      players[id].x = 0;
      players[id].y = 0;
      players[id].items = [];
      players[id].cheerTargetId = null;
      players[id].vote = null;
    }
  });

  stopServerPhysicsLoop();
  broadcastState();
  res.json({ success: true, gameState });
});

// Admin command: Add Bots
app.post('/api/admin/add-bots', (req, res) => {
  const count = Number(req.body.count) || 5;
  const botNames = ['토끼', '호랑이', '사자', '곰', '여우', '늑대', '독수리', '부엉이', '람쥐', '거북이'];
  const presetColors = ['#ff4d4d', '#3b82f6', '#10b981', '#f59e0b', '#8b5cf6', '#ec4899'];

  for (let i = 0; i < count; i++) {
    const playerId = 'bot_' + Math.random().toString(36).substr(2, 5);
    const randomName = botNames[Math.floor(Math.random() * botNames.length)] + Math.floor(Math.random() * 100);
    const randomColor = presetColors[Math.floor(Math.random() * presetColors.length)];
    players[playerId] = {
      playerId,
      nickname: '[BOT] ' + randomName,
      color: randomColor,
      state: (gameState === 'Lobby' || gameState === 'Shop') ? 'Joined' : 'Spectator',
      x: 0,
      y: 0,
      posX: 10 + Math.random() * 80,
      posY: 10 + Math.random() * 80,
      hp: 100,
      maxHp: 100,
      connected: true,
      joinedAt: Date.now(),
      isBot: true,
      items: [],
      cheerTargetId: null,
      vote: null
    };
  }

  broadcastState();
  res.json({ success: true, count: Object.values(players).filter(p => p.isBot).length });
});

// Check if only one player is left alive
function checkGameEndingCondition() {
  if (gameState !== 'Playing') return;

  const alivePlayers = Object.values(players).filter(p => p.state === 'Alive');
  
  if (alivePlayers.length === 1) {
    // We have a winner!
    winner = alivePlayers[0];
    winner.state = 'Winner';
    gameState = 'Result';
    stopServerPhysicsLoop();
  } else if (alivePlayers.length === 0) {
    // Everyone died at the same time?
    gameState = 'Result';
    stopServerPhysicsLoop();
  }
}

// --- WebSocket Event Handlers ---

io.on('connection', (socket) => {
  let socketPlayerId = null;

  console.log(`Socket connected: ${socket.id}`);

  // Send current state on connection
  socket.emit('gameStateChanged', {
    gameState,
    players: Object.values(players),
    ranking,
    winner,
    shopItems,
    voteCounts,
    cheerCounts
  });

  // Client Join
  socket.on('join', ({ nickname, color }, callback) => {
    if (!nickname || nickname.trim() === '') {
      return callback && callback({ success: false, reason: 'Nickname is required' });
    }

    const playerId = generatePlayerId();
    const playerState = (gameState === 'Lobby') ? 'Joined' : 'Spectator';

    players[playerId] = {
      playerId,
      nickname: nickname.trim().substring(0, 8),
      color: color || '#ff4d4d',
      state: playerState,
      x: 0,
      y: 0,
      posX: 15 + Math.random() * 70,
      posY: 15 + Math.random() * 70,
      hp: 100,
      maxHp: 100,
      connected: true,
      joinedAt: Date.now(),
      items: [],
      cheerTargetId: null,
      vote: null
    };

    socketPlayerId = playerId;
    socket.join('players');

    broadcastState();

    if (callback) {
      callback({
        success: true,
        playerId,
        nickname: players[playerId].nickname,
        state: playerState
      });
    }
  });

  // Rejoin/Reconnect
  socket.on('rejoin', ({ playerId }, callback) => {
    if (playerId && players[playerId]) {
      socketPlayerId = playerId;
      players[playerId].connected = true;
      socket.join('players');
      broadcastState();
      if (callback) {
        callback({
          success: true,
          playerId,
          nickname: players[playerId].nickname,
          state: players[playerId].state
        });
      }
    } else {
      if (callback) callback({ success: false, reason: 'Player session not found' });
    }
  });

  // Receive player movement input
  socket.on('moveInput', ({ moveX, moveY }) => {
    if (!socketPlayerId || !players[socketPlayerId]) return;
    if (!canAcceptPlayerInput(socketPlayerId)) return;

    players[socketPlayerId].x = Number(moveX) || 0;
    players[socketPlayerId].y = Number(moveY) || 0;

    // Pushes changes to host dashboard and Unreal client
    io.emit('inputsUpdated', {
      playerId: socketPlayerId,
      moveX: players[socketPlayerId].x,
      moveY: players[socketPlayerId].y
    });

    sendToUnreal({
      type: 'inputsUpdated',
      inputs: [
        {
          playerId: socketPlayerId,
          moveX: players[socketPlayerId].x,
          moveY: players[socketPlayerId].y
        }
      ]
    });
  });

  // Shop Item Purchase
  socket.on('buyItem', ({ playerId, itemId }, callback) => {
    if (gameState !== 'Shop') {
      return callback && callback({ success: false, reason: 'NotShopState' });
    }
    if (!playerId || !players[playerId]) {
      return callback && callback({ success: false, reason: 'PlayerNotFound' });
    }
    if (!itemId || !shopItems[itemId]) {
      return callback && callback({ success: false, reason: 'ItemNotFound' });
    }
    if (shopItems[itemId].stock <= 0) {
      return callback && callback({ success: false, reason: 'SoldOut' });
    }

    shopItems[itemId].stock -= 1;
    if (!players[playerId].items) players[playerId].items = [];
    players[playerId].items.push(itemId);

    broadcastState();
    if (callback) {
      callback({
        success: true,
        items: players[playerId].items
      });
    }
  });

  // Spectator Vote
  socket.on('voteEvent', ({ playerId, eventType }) => {
    if (gameState !== 'Playing') return;
    if (!playerId || !players[playerId]) return;
    
    const p = players[playerId];
    if (p.state !== 'Dead' && p.state !== 'Spectator') return;

    p.vote = eventType;

    // Recalculate vote counts
    voteCounts = { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
    Object.values(players).forEach(pl => {
      if ((pl.state === 'Dead' || pl.state === 'Spectator') && pl.vote) {
        if (voteCounts[pl.vote] !== undefined) {
          voteCounts[pl.vote]++;
        }
      }
    });

    broadcastState();
  });

  // Spectator Cheer
  socket.on('cheer', ({ playerId, targetId }) => {
    if (gameState !== 'Playing') return;
    if (!playerId || !players[playerId]) return;
    if (!targetId || !players[targetId]) return;

    const p = players[playerId];
    if (p.state !== 'Dead' && p.state !== 'Spectator') return;

    const target = players[targetId];
    if (target.state !== 'Alive') return;

    p.cheerTargetId = targetId;

    // Recalculate cheer counts
    cheerCounts = {};
    Object.values(players).forEach(pl => {
      if ((pl.state === 'Dead' || pl.state === 'Spectator') && pl.cheerTargetId) {
        cheerCounts[pl.cheerTargetId] = (cheerCounts[pl.cheerTargetId] || 0) + 1;
      }
    });

    broadcastState();
  });

  // Admin controls via Websocket
  socket.on('adminStartShop', () => {
    if (gameState !== 'Lobby') return;
    gameState = 'Shop';
    shopItems = JSON.parse(JSON.stringify(INITIAL_SHOP_ITEMS));
    
    Object.keys(players).forEach(id => {
      players[id].items = [];
      players[id].cheerTargetId = null;
      players[id].vote = null;
    });

    broadcastState();
  });

  socket.on('adminStartGame', () => {
    if (gameState !== 'Lobby' && gameState !== 'Shop') return;
    gameState = 'Playing';
    ranking = [];
    winner = null;

    Object.keys(players).forEach(id => {
      if (players[id].state === 'Joined' || players[id].state === 'Ready') {
        players[id].state = 'Alive';
      } else {
        players[id].state = 'Spectator';
      }
      players[id].x = 0;
      players[id].y = 0;
    });

    broadcastState();
    startServerPhysicsLoop();
  });

  socket.on('adminResetGame', () => {
    gameState = 'Lobby';
    ranking = [];
    winner = null;

    shopItems = JSON.parse(JSON.stringify(INITIAL_SHOP_ITEMS));
    voteCounts = { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
    cheerCounts = {};

    Object.keys(players).forEach(id => {
      if (players[id].isBot) {
        delete players[id];
      } else {
        players[id].state = 'Joined';
      players[id].x = 0;
      players[id].y = 0;
      players[id].hp = 100;
      players[id].maxHp = players[id].maxHp || 100;
      players[id].items = [];
        players[id].cheerTargetId = null;
        players[id].vote = null;
      }
    });

    stopServerPhysicsLoop();
    broadcastState();
  });

  socket.on('adminAddBots', ({ count }) => {
    const botCount = Number(count) || 5;
    const botNames = ['토끼', '호랑이', '사자', '곰', '여우', '늑대', '독수리', '부엉이', '람쥐', '거북이'];
    const presetColors = ['#ff4d4d', '#3b82f6', '#10b981', '#f59e0b', '#8b5cf6', '#ec4899'];

    for (let i = 0; i < botCount; i++) {
      const playerId = 'bot_' + Math.random().toString(36).substr(2, 5);
      const randomName = botNames[Math.floor(Math.random() * botNames.length)] + Math.floor(Math.random() * 100);
      const randomColor = presetColors[Math.floor(Math.random() * presetColors.length)];
      players[playerId] = {
        playerId,
        nickname: '[BOT] ' + randomName,
        color: randomColor,
        state: (gameState === 'Lobby' || gameState === 'Shop') ? 'Joined' : 'Spectator',
      x: 0,
      y: 0,
      posX: 10 + Math.random() * 80,
      posY: 10 + Math.random() * 80,
      hp: 100,
      maxHp: 100,
      connected: true,
        joinedAt: Date.now(),
        isBot: true,
        items: [],
        cheerTargetId: null,
        vote: null
      };
    }
    broadcastState();
  });

  // Unreal client reports death or state change
  socket.on('unrealPlayerStateChanged', ({ playerId, state }) => {
    if (playerId && players[playerId]) {
      const oldState = players[playerId].state;
      players[playerId].state = state;

      if (state === 'Dead' && oldState !== 'Dead') {
        ranking.push({
          playerId,
          nickname: players[playerId].nickname,
          timeOfDeath: Date.now()
        });
        io.emit('playerDead', { playerId, nickname: players[playerId].nickname });
        checkGameEndingCondition();
      }
      broadcastState();
    }
  });

  // Disconnection handler
  socket.on('disconnect', () => {
    console.log(`Socket disconnected: ${socket.id}`);
    if (socketPlayerId && players[socketPlayerId]) {
      // Keep player, but mark as disconnected so they can reconnect.
      players[socketPlayerId].connected = false;
      // If the game is in Lobby, we can just delete them if they disconnect.
      if (gameState === 'Lobby') {
        delete players[socketPlayerId];
      }
      broadcastState();
    }
  });
});

// Pure WebSocket Server for Unreal Engine (sharing port 3000 on path /ws/unreal)
const wss = new WebSocket.Server({ noServer: true });

wss.on('connection', (ws) => {
  console.log('[Unreal WS] Client connected via WebSocket');
  unrealSocket = ws;

  // Send current state immediately on connection
  sendToUnreal({
    type: 'gameStateChanged',
    gameState,
    players: Object.values(players),
    ranking,
    winner,
    shopItems,
    voteCounts,
    cheerCounts
  });

  ws.on('message', (message) => {
    try {
      const data = JSON.parse(message);
      if (data.type === 'worldState') {
        const reportedPlayers = Array.isArray(data.players) ? data.players : [];
        lastUnrealWorldStateAt = Date.now();
        let shouldBroadcastState = false;

        reportedPlayers.forEach(snapshot => {
          const playerId = snapshot && snapshot.playerId;
          if (!playerId || !players[playerId]) return;

          const worldX = Number(snapshot.worldX);
          const worldY = Number(snapshot.worldY);
          if (Number.isFinite(worldX) && Number.isFinite(worldY)) {
            const percentPosition = worldToPercent(worldX, worldY);
            players[playerId].worldX = worldX;
            players[playerId].worldY = worldY;
            players[playerId].posX = percentPosition.posX;
            players[playerId].posY = percentPosition.posY;
          }

          const hp = Number(snapshot.hp);
          const maxHp = Number(snapshot.maxHp);
          if (Number.isFinite(hp)) {
            players[playerId].hp = Math.max(0, hp);
          }
          if (Number.isFinite(maxHp) && maxHp > 0) {
            players[playerId].maxHp = maxHp;
          }

          if (snapshot.alive === false || players[playerId].hp <= 0) {
            shouldBroadcastState = markPlayerDead(playerId) || shouldBroadcastState;
          } else if (players[playerId].state !== 'Dead' && players[playerId].state !== 'Winner') {
            shouldBroadcastState = players[playerId].state !== 'Alive' || shouldBroadcastState;
            players[playerId].state = 'Alive';
          }
        });

        io.emit('positionsUpdated', buildCompactPositions());
        if (shouldBroadcastState) {
          broadcastState();
        }
      }
    } catch (err) {
      console.error('[Unreal WS] Failed to parse message:', err);
    }
  });

  ws.on('close', () => {
    console.log('[Unreal WS] Client disconnected');
    if (unrealSocket === ws) {
      unrealSocket = null;
    }
  });

  ws.on('error', (err) => {
    console.error('[Unreal WS] Socket error:', err);
  });
});

// Intercept HTTP upgrade to route /ws/unreal to pure ws server
server.on('upgrade', (request, socket, head) => {
  const pathname = url.parse(request.url).pathname;
  if (pathname === '/ws/unreal') {
    wss.handleUpgrade(request, socket, head, (ws) => {
      wss.emit('connection', ws, request);
    });
  }
});

// Start Server
server.listen(PORT, () => {
  const localIp = getLocalIpAddress();
  console.log(`===================================================`);
  console.log(`Showdown Live Server running on port ${PORT}`);
  console.log(`Access locally at:`);
  console.log(`- Mobile Client: http://localhost:${PORT}`);
  console.log(`- Host Dashboard: http://localhost:${PORT}/host.html`);
  console.log(`---------------------------------------------------`);
  console.log(`Access from same Wi-Fi / LAN network at:`);
  console.log(`- Mobile Client: http://${localIp}:${PORT}`);
  console.log(`- Host Dashboard: http://${localIp}:${PORT}/host.html`);
  console.log(`===================================================`);

  // Start Cloudflare Tunnel for direct, seamless global remote access
  console.log('Establishing public internet tunnel via Cloudflare Tunnel (cloudflared)...');
  
  // npx cloudflared tunnel --url http://localhost:PORT
  tunnelProcess = spawn('npx', ['cloudflared', 'tunnel', '--url', `http://localhost:${PORT}`], { shell: true });

  const handleTunnelLog = (data) => {
    const output = data.toString();
    const match = output.match(/https:\/\/[a-z0-9-]+\.trycloudflare\.com/);
    if (match) {
      tunnelUrl = match[0].trim();
      console.log(`===================================================`);
      console.log(`[Cloudflare] PUBLIC REMOTE ACCESS ESTABLISHED!`);
      console.log(`- Public URL: ${tunnelUrl}`);
      console.log(`- Host Dashboard: ${tunnelUrl}/host.html`);
      console.log(`===================================================`);
    }
  };

  tunnelProcess.stdout.on('data', handleTunnelLog);
  tunnelProcess.stderr.on('data', handleTunnelLog);

  tunnelProcess.on('close', (code) => {
    console.log(`[Cloudflare] Tunnel process exited with code ${code}`);
    tunnelUrl = null;
  });
});

// Clean up tunnel process on exit
const cleanUp = () => {
  if (tunnelProcess) {
    console.log('Killing Cloudflare Tunnel process...');
    tunnelProcess.kill();
  }
  process.exit();
};
process.on('SIGINT', cleanUp);
process.on('SIGTERM', cleanUp);
