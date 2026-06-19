// Host Dashboard Application Logic

document.addEventListener('DOMContentLoaded', () => {
  const socket = io();

  // DOM Elements
  const gameStateBadge = document.getElementById('game-state-badge');
  const aliveCountEl = document.getElementById('alive-count');
  const totalCountEl = document.getElementById('total-count');
  
  const qrCodeImg = document.getElementById('qr-code-img');
  const qrPlaceholder = document.getElementById('qr-placeholder');
  const connectionUrlEl = document.getElementById('connection-url');
  
  const btnShop = document.getElementById('btn-shop');
  const btnStart = document.getElementById('btn-start');
  const btnReset = document.getElementById('btn-reset');
  const botCountInput = document.getElementById('bot-count');
  const btnAddBots = document.getElementById('btn-add-bots');

  // Shop is disabled on the Spring MVP backend until shop APIs are implemented.
  if (btnShop) {
    btnShop.disabled = true;
    btnShop.title = 'Spring MVP backend does not support shop yet.';
  }
  
  const playersGrid = document.getElementById('players-grid');
  const winnerBannerWrapper = document.getElementById('winner-banner-wrapper');
  const winnerNameEl = document.getElementById('winner-name');
  const unrealStatusEl = document.getElementById('unreal-connector-status');
  const gameArena = document.getElementById('game-arena');
  const arenaWrapper = document.getElementById('arena-wrapper');

  // Cache player DOM elements for high-performance direct updates
  let playerCardElements = {}; 
  let currentPlayers = {};
  let arenaPlayerElements = {}; // playerId -> DOM element in arena
  let gameLoopInterval = null;
  let lastGameState = null;
  let lastRosterSignature = '';
  const MOVE_SPEED = 0.8; // Speed coefficient for 2D arena movement

  function makeRosterSignature(players) {
    return players
      .map(p => `${p.playerId}:${p.nickname}:${p.state}:${p.connected}:${p.isBot}:${p.color || ''}:${(p.items || []).join(',')}`)
      .join('|');
  }

  function playerColor(player) {
    return /^#[0-9a-fA-F]{6}$/.test(player && player.color ? player.color : '') ? player.color : 'var(--color-blue)';
  }

  function stateLabel(state) {
    if (state === 'Joined') return '대기중';
    if (state === 'Alive') return '생존';
    if (state === 'Dead') return '탈락';
    if (state === 'Spectator') return '관전';
    if (state === 'Winner') return '우승';
    return state || '-';
  }

  // --- 1. QR Code & URL Generation ---
  const currentHostname = window.location.hostname;
  let joinUrl = window.location.origin;
  connectionUrlEl.textContent = joinUrl;

  // Fetch host network info from server to automatically handle localhost/tunnels
  fetch('/api/host-info')
    .then(res => res.json())
    .then(data => {
      const serverLanIp = data.localIp;
      const port = data.port;
      const tunnelUrl = data.tunnelUrl;
      
      if (tunnelUrl) {
        // If tunnel is open, prioritize it as the join URL (works everywhere on any network)
        joinUrl = tunnelUrl;
        connectionUrlEl.textContent = joinUrl;
        
        // Add a helper text showing tunnel is active
        const tunnelText = document.createElement('p');
        tunnelText.style.color = 'var(--color-mint-text)';
        tunnelText.style.fontSize = '0.85rem';
        tunnelText.style.marginTop = '8px';
        tunnelText.style.fontWeight = '600';
        tunnelText.innerHTML = `🌐 외부 인터넷 접속 터널 활성화 완료!<br>(3G/4G/5G/LTE 및 다른 와이파이 환경에서도 자유롭게 접속 가능)`;
        connectionUrlEl.parentNode.appendChild(tunnelText);
      } else if (currentHostname === 'localhost' || currentHostname === '127.0.0.1') {
        // If tunnel failed, fall back to LAN IP warning (requires same Wi-Fi)
        joinUrl = `http://${serverLanIp}:${port}`;
        connectionUrlEl.textContent = joinUrl;
        
        // Add a warning helper below the URL
        const warningText = document.createElement('p');
        warningText.style.color = 'var(--color-peach-text)';
        warningText.style.fontSize = '0.8rem';
        warningText.style.marginTop = '8px';
        warningText.style.fontWeight = '600';
        warningText.innerHTML = `⚠️ 터널 미개설: 모바일 기기 접속을 위해 스마트폰을 <b>동일한 Wi-Fi</b>에 연결해 주세요.<br>(또는 PC에서 <a href="${joinUrl}/host.html" style="color: var(--color-blue-text); text-decoration: underline;">이 링크</a>로 다시 접속하세요)`;
        connectionUrlEl.parentNode.appendChild(warningText);
      }
      
      // Load QR code with the best available join URL
      qrCodeImg.src = `https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=${encodeURIComponent(joinUrl)}`;
      qrCodeImg.onload = () => {
        qrPlaceholder.style.display = 'none';
        qrCodeImg.style.display = 'block';
      };
    })
    .catch(err => {
      console.error('Failed to fetch host network info, falling back to window.location.origin:', err);
      // Fallback to page location if API fails
      qrCodeImg.src = `https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=${encodeURIComponent(joinUrl)}`;
      qrCodeImg.onload = () => {
        qrPlaceholder.style.display = 'none';
        qrCodeImg.style.display = 'block';
      };
    });

  // --- 2. Socket Connection Handlers ---

  socket.on('connect', () => {
    unrealStatusEl.textContent = '웹소켓 서버 연결됨';
    unrealStatusEl.style.color = 'var(--color-mint-text)';
  });

  socket.on('disconnect', () => {
    unrealStatusEl.textContent = '웹소켓 서버 끊김';
    unrealStatusEl.style.color = 'var(--color-peach-text)';
  });

  socket.on('gameStateChanged', (data) => {
    console.log('Host state update:', data);
    const playersList = data.players || [];
    const rosterSignature = makeRosterSignature(playersList);
    const stateChanged = lastGameState !== data.gameState;
    const rosterChanged = lastRosterSignature !== rosterSignature;

    const activeIds = new Set();
    playersList.forEach(p => {
      activeIds.add(p.playerId);
      currentPlayers[p.playerId] = { ...(currentPlayers[p.playerId] || {}), ...p };
    });
    Object.keys(currentPlayers).forEach(id => {
      if (!activeIds.has(id)) delete currentPlayers[id];
    });

    // Update Header Badges
    gameStateBadge.textContent = data.gameState;
    gameStateBadge.className = 'stat-value';
    
    if (data.gameState === 'Lobby') {
      gameStateBadge.style.color = 'var(--color-blue-text)';
      winnerBannerWrapper.style.display = 'none';
      arenaWrapper.style.display = 'none';
      btnShop.disabled = false;
      btnStart.disabled = false;
      if (stateChanged) stopGameLoop();
    } else if (data.gameState === 'Shop') {
      gameStateBadge.style.color = 'var(--color-mint-text)';
      winnerBannerWrapper.style.display = 'none';
      arenaWrapper.style.display = 'none';
      btnShop.disabled = true;
      btnStart.disabled = false;
      if (stateChanged) stopGameLoop();
    } else if (data.gameState === 'Playing') {
      gameStateBadge.style.color = 'var(--color-peach-text)';
      winnerBannerWrapper.style.display = 'none';
      arenaWrapper.style.display = 'block';
      btnShop.disabled = true;
      btnStart.disabled = true;
      if (stateChanged || rosterChanged) startGameLoop(playersList);
    } else if (data.gameState === 'Result') {
      gameStateBadge.style.color = 'var(--color-coral-text)';
      btnShop.disabled = true;
      btnStart.disabled = true;
      arenaWrapper.style.display = 'none';
      if (stateChanged) stopGameLoop();

      // Show final winner if available
      if (data.winner) {
        winnerNameEl.textContent = data.winner.nickname;
        winnerBannerWrapper.style.display = 'block';
      }
    }

    // Update Player Counts
    const totalCount = playersList.length;
    const aliveCount = playersList.filter(p => p.state === 'Alive').length;
    
    totalCountEl.textContent = totalCount;
    aliveCountEl.textContent = `${aliveCount} / ${totalCount}`;

    // Rebuild player cards grid only when roster/state identity actually changes.
    if (rosterChanged) rebuildPlayersGrid(playersList);

    // Update live vote counts
    const voteCounts = data.voteCounts || { HealZone: 0, SpeedUp: 0, ShrinkZone: 0, SupplyBox: 0 };
    Object.keys(voteCounts).forEach(key => {
      const el = document.getElementById(`vote-count-${key}`);
      if (el) {
        el.textContent = `${voteCounts[key]}표`;
      }
    });

    lastGameState = data.gameState;
    lastRosterSignature = rosterSignature;
  });

  // Smooth real-time update of player inputs (60fps friendly)
  socket.on('inputsUpdated', (data) => {
    const { playerId, moveX, moveY } = data;
    
    // Update local cache
    if (currentPlayers[playerId]) {
      currentPlayers[playerId].x = moveX;
      currentPlayers[playerId].y = moveY;
    }

    // Direct DOM manipulation instead of grid rebuilds
    const dot = document.getElementById(`joy-dot-${playerId}`);
    const text = document.getElementById(`joy-txt-${playerId}`);
    
    if (dot && text) {
      // Map vector values to positioning offset (max 20px radius)
      const offsetPx = 22; // half of mini-joystick boundary
      const xOffset = moveX * offsetPx;
      const yOffset = -moveY * offsetPx; // Negate Y since screen coordinates are inverted

      dot.style.left = `calc(50% + ${xOffset}px)`;
      dot.style.top = `calc(50% + ${yOffset}px)`;

      // Active highlighting
      if (moveX !== 0 || moveY !== 0) {
        dot.classList.add('active');
        dot.style.backgroundColor = currentPlayers[playerId].color || 'var(--color-mint)';
      } else {
        dot.classList.remove('active');
        dot.style.backgroundColor = currentPlayers[playerId].color || 'var(--color-blue)';
      }

      // Update values text
      text.textContent = `X: ${moveX.toFixed(2)}, Y: ${moveY.toFixed(2)}`;
    }
  });

  // Receive player coordinates from server physics loop
  socket.on('positionsUpdated', (positions) => {
    Object.keys(positions).forEach(id => {
      const pos = positions[id];
      if (currentPlayers[id]) {
        currentPlayers[id].serverX = pos[0];
        currentPlayers[id].serverY = pos[1];
        currentPlayers[id].hp = pos[3];
        currentPlayers[id].maxHp = pos[4];
        
        // Fallback initialization if needed
        if (currentPlayers[id].posX === undefined) {
          currentPlayers[id].posX = pos[0];
          currentPlayers[id].posY = pos[1];
        }
      }
    });
  });

  // --- 3. Grid Renderer ---

  function rebuildPlayersGrid(players) {
    playersGrid.innerHTML = '';
    playerCardElements = {};

    if (players.length === 0) {
      playersGrid.innerHTML = `
        <div style="grid-column: 1/-1; text-align: center; padding: 40px; color: var(--text-secondary);">
          <p style="font-size: 1.1rem; font-weight: 500;">현재 접속한 참가자가 없습니다.</p>
          <p style="font-size: 0.85rem; margin-top: 4px;">왼쪽 QR코드를 스마트폰으로 스캔해 접속을 유도하세요!</p>
        </div>
      `;
      return;
    }

    players.forEach(p => {
      const card = document.createElement('div');
      card.className = `player-dashboard-card ${p.state.toLowerCase()}`;
      card.id = `player-card-${p.playerId}`;
      card.style.setProperty('--player-color', playerColor(p));

      // Header info
      const cardHeader = document.createElement('div');
      cardHeader.className = 'player-card-header';

      const nameWrap = document.createElement('div');
      nameWrap.className = 'player-name-wrap';

      const colorDot = document.createElement('span');
      colorDot.className = 'player-color-dot';
      
      const nameEl = document.createElement('span');
      nameEl.className = 'player-name';
      nameEl.textContent = p.nickname;
      nameEl.title = p.nickname;
      
      const badgeEl = document.createElement('span');
      badgeEl.className = `player-card-badge badge-${p.state.toLowerCase()}`;
      badgeEl.textContent = stateLabel(p.state);

      nameWrap.appendChild(colorDot);
      nameWrap.appendChild(nameEl);
      cardHeader.appendChild(nameWrap);
      cardHeader.appendChild(badgeEl);

      // Append header
      card.appendChild(cardHeader);

      // miniature joystick visualizer
      const joyWrapper = document.createElement('div');
      joyWrapper.className = 'mini-joystick-wrapper';
      
      const joyDot = document.createElement('div');
      joyDot.className = 'mini-joystick-dot';
      joyDot.id = `joy-dot-${p.playerId}`;
      joyDot.style.backgroundColor = playerColor(p);

      // Set initial dot position
      const offsetPx = 22;
      const initX = p.x * offsetPx;
      const initY = -p.y * offsetPx;
      joyDot.style.left = `calc(50% + ${initX}px)`;
      joyDot.style.top = `calc(50% + ${initY}px)`;
      if (p.x !== 0 || p.y !== 0) {
        joyDot.classList.add('active');
      }

      joyWrapper.appendChild(joyDot);
      card.appendChild(joyWrapper);

      // vector debug text
      const vectorText = document.createElement('div');
      vectorText.className = 'player-vector-text';
      vectorText.id = `joy-txt-${p.playerId}`;
      vectorText.textContent = `X: ${(p.x || 0).toFixed(2)}, Y: ${(p.y || 0).toFixed(2)}`;
      card.appendChild(vectorText);

      // Render player items
      if (p.items && p.items.length > 0) {
        const itemsWrapper = document.createElement('div');
        itemsWrapper.className = 'player-card-items';
        itemsWrapper.style.display = 'flex';
        itemsWrapper.style.gap = '4px';
        itemsWrapper.style.marginTop = '6px';
        itemsWrapper.style.justifyContent = 'center';
        
        p.items.forEach(itemId => {
          const badge = document.createElement('span');
          badge.style.display = 'inline-block';
          badge.style.padding = '2px 6px';
          badge.style.borderRadius = '3px';
          badge.style.fontSize = '0.75rem';
          badge.style.background = 'var(--color-mint-light)';
          badge.style.border = '1px solid var(--color-mint-border)';
          badge.style.color = 'var(--color-mint-text)';
          badge.style.fontWeight = 'bold';
          
          let emoji = itemId;
          if (itemId === 'atk_boost') emoji = '⚔️';
          else if (itemId === 'speed_boost') emoji = '⚡';
          else if (itemId === 'range_boost') emoji = '🎯';
          else if (itemId === 'shield') emoji = '🛡️';
          
          badge.textContent = emoji;
          badge.title = itemId;
          itemsWrapper.appendChild(badge);
        });
        card.appendChild(itemsWrapper);
      }

      // Action buttons: Manual elimination (Kick/Eliminate button)
      if (p.state === 'Alive') {
        const btnKick = document.createElement('button');
        btnKick.className = 'btn-kick';
        btnKick.innerHTML = '💀 탈락';
        btnKick.style.marginTop = '10px';
        btnKick.addEventListener('click', () => {
          eliminatePlayer(p.playerId);
        });
        card.appendChild(btnKick);
      }

      playersGrid.appendChild(card);
      playerCardElements[p.playerId] = card;
    });
  }

  // Eliminate player via Host request
  function eliminatePlayer(playerId) {
    if (confirm('해당 플레이어를 즉시 탈락(사망)시키겠습니까?')) {
      // POST request to /api/unreal/player-state
      fetch('/api/unreal/player-state', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          playerId: playerId,
          state: 'Dead'
        })
      })
      .then(res => res.json())
      .then(data => {
        if (!data.success) {
          alert('상태 갱신 실패');
        }
      })
      .catch(err => {
        console.error('Error eliminating player:', err);
      });
    }
  }

  // --- 2D Arena Live Preview Simulation Loop ---

  function startGameLoop(players) {
    gameArena.innerHTML = '';
    arenaPlayerElements = {};

    players.forEach(p => {
      // Initialize coordinate values inside the arena container
      if (currentPlayers[p.playerId]) {
        currentPlayers[p.playerId].posX = p.posX || 15 + Math.random() * 70;
        currentPlayers[p.playerId].posY = p.posY || 15 + Math.random() * 70;
        currentPlayers[p.playerId].serverX = currentPlayers[p.playerId].posX;
        currentPlayers[p.playerId].serverY = currentPlayers[p.playerId].posY;
        currentPlayers[p.playerId].x = p.x || 0;
        currentPlayers[p.playerId].y = p.y || 0;
      }

      // Create DOM element for simulated player
      const playerEl = document.createElement('div');
      playerEl.className = `arena-player ${p.state.toLowerCase()}`;
      if (p.isBot) playerEl.classList.add('bot');
      playerEl.id = `arena-player-${p.playerId}`;
      playerEl.style.setProperty('--player-color', playerColor(p));
      playerEl.style.left = `${currentPlayers[p.playerId].posX}%`;
      playerEl.style.top = `${currentPlayers[p.playerId].posY}%`;

      const dot = document.createElement('span');
      dot.className = 'arena-player-dot';

      const name = document.createElement('span');
      name.className = 'arena-player-name';
      name.textContent = p.nickname;

      playerEl.appendChild(dot);
      playerEl.appendChild(name);
      gameArena.appendChild(playerEl);

      arenaPlayerElements[p.playerId] = playerEl;
    });

    if (gameLoopInterval) clearInterval(gameLoopInterval);
    gameLoopInterval = setInterval(updatePlayerPositions, 33); // Run loop at ~30 FPS
  }

  function stopGameLoop() {
    if (gameLoopInterval) {
      clearInterval(gameLoopInterval);
      gameLoopInterval = null;
    }
    gameArena.innerHTML = '';
    arenaPlayerElements = {};
  }

  function updatePlayerPositions() {
    Object.keys(currentPlayers).forEach(id => {
      const p = currentPlayers[id];
      if (!p) return;

      const el = arenaPlayerElements[id];
      if (!el) return;

      // Handle dead or spectating players
      if (p.state !== 'Alive' && p.state !== 'Joined') {
        if (!el.classList.contains('dead')) {
          el.classList.remove('alive');
          el.classList.add('dead');
        }
        return;
      }

      // Smoothly interpolate towards the server's coordinates (lerp speed 0.15)
      if (p.serverX !== undefined && p.serverY !== undefined) {
        p.posX = p.posX + (p.serverX - p.posX) * 0.15;
        p.posY = p.posY + (p.serverY - p.posY) * 0.15;

        // Update DOM positioning
        el.style.left = `${p.posX}%`;
        el.style.top = `${p.posY}%`;

        // Highlight if currently moving
        const isMoving = p.x !== 0 || p.y !== 0 || Math.abs(p.serverX - p.posX) > 0.05;
        if (isMoving) {
          if (!el.classList.contains('alive')) {
            el.classList.add('alive');
          }
        } else {
          el.classList.remove('alive');
        }
      }
    });
  }

  // --- 4. Controls Handlers ---

  btnShop.addEventListener('click', () => {
    socket.emit('adminStartShop');
  });

  btnStart.addEventListener('click', () => {
    socket.emit('adminStartGame');
  });

  btnReset.addEventListener('click', () => {
    if (confirm('모든 데이터를 리셋하고 대기방으로 돌아가겠습니까?')) {
      socket.emit('adminResetGame');
    }
  });

  btnAddBots.addEventListener('click', () => {
    const count = Number(botCountInput.value) || 5;
    socket.emit('adminAddBots', { count });
  });
});


