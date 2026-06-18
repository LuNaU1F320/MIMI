// Mobile Client Application Logic

// Global debug logger directly on mobile UI for presentation testing
window.onerror = function(msg, url, line, col, error) {
  const statusEl = document.getElementById('connection-status');
  if (statusEl) {
    statusEl.textContent = `⚠️ 오류: ${msg} (${line}행)`;
    statusEl.style.color = 'var(--color-peach-text)';
  }
  console.error("Mobile error:", msg, "at line:", line, error);
  return false;
};

document.addEventListener('DOMContentLoaded', () => {
  // Prevent double-tap to zoom in mobile browsers
  let lastTouchEnd = 0;
  document.addEventListener('touchend', (event) => {
    const now = Date.now();
    if (now - lastTouchEnd <= 300) {
      event.preventDefault();
    }
    lastTouchEnd = now;
  }, { passive: false });

  // Prevent pinch-to-zoom (multi-touch zoom)
  document.addEventListener('touchstart', (event) => {
    if (event.touches.length > 1) {
      event.preventDefault();
    }
  }, { passive: false });

  const socket = io();

  // DOM Elements
  const statusEl = document.getElementById('connection-status');
  const screenJoin = document.getElementById('screen-join');
  const screenLobby = document.getElementById('screen-lobby');
  const screenShop = document.getElementById('screen-shop');
  const shopItemsContainer = document.getElementById('shop-items');
  const myItemsListEl = document.getElementById('my-items-list');
  const screenPlaying = document.getElementById('screen-playing');
  const screenResult = document.getElementById('screen-result');
  const nicknameInput = document.getElementById('nickname-input');
  const btnJoin = document.getElementById('btn-join');
  const playerChips = document.getElementById('player-chips');
  const playerCountEl = document.getElementById('player-count');
  
  const playingNickname = document.getElementById('playing-nickname');
  const playingStatus = document.getElementById('playing-status');
  const healthValue = document.getElementById('health-value');
  const healthBar = document.getElementById('health-bar');
  const joystickTrack = document.getElementById('joystick-track');
  const joystickKnob = document.getElementById('joystick-knob');
  const joystickDebug = document.getElementById('joystick-debug');
  const btnJump = document.getElementById('btn-jump');
  const btnEmote = document.getElementById('btn-emote');
  const spectatorPanel = document.getElementById('spectator-panel');
  
  const resultBadge = document.getElementById('result-badge');
  const resultTitle = document.getElementById('result-title');
  const resultRank = document.getElementById('result-rank');
  const resultStateTxt = document.getElementById('result-state-txt');
  const resultTimeTxt = document.getElementById('result-time-txt');
  const btnLobbyReturn = document.getElementById('btn-lobby-return');
  
  const minimapCanvas = document.getElementById('minimap-canvas');
  const minimapCtx = minimapCanvas ? minimapCanvas.getContext('2d') : null;

  // Application State
  let myPlayerId = sessionStorage.getItem('showdown_playerId') || null;
  let myNickname = sessionStorage.getItem('showdown_nickname') || null;
  let myState = 'Joined'; // 'Joined' | 'Alive' | 'Dead' | 'Spectator'
  let currentGameState = 'Lobby';
  let joystickInstance = null;
  let inputInterval = null;
  let lastSentVector = { x: 0, y: 0 };
  let jumpSeq = 0;
  let emoteSeq = 0;
  let playerPositions = {};
  let currentPlayersList = [];
  const RADAR_VISION_RANGE_UNITS = 1300; // Radar vision range in Unreal Units (6000 * 22% = 1320)


  function syncActionButtons() {
    const disabled = myState !== 'Alive' || currentGameState !== 'Playing';
    if (btnJump) btnJump.disabled = disabled;
    if (btnEmote) btnEmote.disabled = disabled;
  }
  // --- 1. Premium Visual Effects ---
  
  // Water ripple effect on screen touch
  document.body.addEventListener('pointerdown', (e) => {
    const rippleArea = document.getElementById('ripple-area');
    if (!rippleArea) return;
    
    const ripple = document.createElement('div');
    ripple.className = 'ripple';
    
    // Position relative to container
    const rect = rippleArea.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    
    ripple.style.left = `${x}px`;
    ripple.style.top = `${y}px`;
    
    rippleArea.appendChild(ripple);
    
    // Remove element after animation completes
    setTimeout(() => {
      ripple.remove();
    }, 600);
  });

  // --- 2. Socket Connection Handlers ---

  socket.on('connect', () => {
    statusEl.textContent = '서버 연결됨';
    statusEl.style.color = 'var(--color-mint-text)';
    
    // Attempt automatic session reconnection
    if (myPlayerId) {
      socket.emit('rejoin', { playerId: myPlayerId }, (response) => {
        if (response.success) {
          console.log('Rejoined successfully:', response);
          myNickname = response.nickname;
          myState = response.state;
        } else {
          // Session expired or server restarted
          clearSession();
          showScreen(screenJoin);
        }
      });
    } else {
      showScreen(screenJoin);
    }
  });

  socket.on('disconnect', () => {
    statusEl.textContent = '서버 연결 끊김... 재연결 시도 중';
    statusEl.style.color = 'var(--color-peach-text)';
  });

  // Handle server-wide state changes
  socket.on('gameStateChanged', (data) => {
    console.log('Game state updated:', data);
    currentGameState = data.gameState;
    
    const playersList = data.players || [];
    currentPlayersList = playersList; // Cache players list details for radar name display
    const myData = playersList.find(p => p.playerId === myPlayerId);

    if (myData) {
      myState = myData.state;
    }

    // Process UI depending on Game State
    switch (currentGameState) {
      case 'Lobby':
        stopInputSending();
        if (myPlayerId) {
          showScreen(screenLobby);
          updateLobbyPlayers(playersList);
        } else {
          showScreen(screenJoin);
        }
        break;

      case 'Shop':
        stopInputSending();
        if (myPlayerId) {
          showScreen(screenShop);
          updateShopItems(data.shopItems || []);
          updateMyItems(myData);
        } else {
          showScreen(screenJoin);
        }
        break;

      case 'Playing':
        if (myPlayerId) {
          showScreen(screenPlaying);
          updatePlayingScreen(myData);
          startInputSending();
        } else {
          // Connected during gameplay -> join spectator mode
          showScreen(screenJoin);
        }
        break;

      case 'Result':
        stopInputSending();
        if (myPlayerId) {
          showScreen(screenResult);
          showGameResults(data);
        } else {
          showScreen(screenJoin);
        }
        break;
    }
  });

  // Player died broadcast
  socket.on('playerDead', (data) => {
    console.log('Player died:', data);
    if (data.playerId === myPlayerId) {
      myState = 'Dead';
      // Trigger haptic feedback if supported
      if (navigator.vibrate) {
        navigator.vibrate([150, 100, 150]);
      }
      // Instantly refresh playing view to load spectator panel
      socket.emit('rejoin', { playerId: myPlayerId }, (res) => {
        if (res.success) myState = res.state;
      });
    }
  });

  // Receive player coordinates broadcast from server physics loop
  socket.on('positionsUpdated', (positions) => {
    playerPositions = positions;
    updateMyHealthFromPositionState();
    if (currentGameState === 'Playing') {
      drawMinimap();
    }
  });

  // --- 3. Screen Navigation & Updates ---

  function showScreen(targetScreen) {
    // Hide all screens
    [screenJoin, screenLobby, screenShop, screenPlaying, screenResult].forEach(screen => {
      if (screen) screen.classList.remove('active');
    });
    
    // Show target screen
    if (targetScreen) targetScreen.classList.add('active');
  }

  function clearSession() {
    sessionStorage.removeItem('showdown_playerId');
    sessionStorage.removeItem('showdown_nickname');
    myPlayerId = null;
    myNickname = null;
    myState = 'Joined';
  }

  // Shop views renderer
  function updateShopItems(items) {
    shopItemsContainer.innerHTML = '';
    if (items.length === 0) {
      shopItemsContainer.innerHTML = '<p style="text-align:center; padding:20px; color:var(--text-secondary);">상점에 아이템이 없습니다.</p>';
      return;
    }

    items.forEach(item => {
      const card = document.createElement('div');
      card.className = 'shop-item-card';
      card.style.display = 'flex';
      card.style.alignItems = 'center';
      card.style.justifyContent = 'space-between';
      card.style.padding = '12px 16px';
      card.style.background = 'var(--bg-secondary)';
      card.style.border = '1px solid var(--bg-tertiary)';
      card.style.borderRadius = 'var(--radius-md)';
      card.style.boxShadow = 'var(--shadow-sm)';
      if (item.stock === 0) {
        card.style.opacity = '0.5';
      }

      const info = document.createElement('div');
      
      const name = document.createElement('p');
      name.style.fontWeight = '700';
      name.style.fontSize = '1.05rem';
      name.style.color = 'var(--text-primary)';
      name.textContent = item.name;
      
      const desc = document.createElement('p');
      desc.style.fontSize = '0.8rem';
      desc.style.color = 'var(--color-blue-text)';
      desc.style.marginTop = '2px';
      
      let effectTxt = '';
      if (item.effect.type === 'AttackPower') effectTxt = `공격력 +${Math.round((item.effect.value - 1) * 100)}% ⚔️`;
      else if (item.effect.type === 'Speed') effectTxt = `이동속도 +${Math.round((item.effect.value - 1) * 100)}% ⚡`;
      else if (item.effect.type === 'Range') effectTxt = `공격범위 +${Math.round((item.effect.value - 1) * 100)}% 🎯`;
      else if (item.effect.type === 'Shield') effectTxt = `보호막 +${item.effect.value} 🛡️`;
      desc.textContent = effectTxt;

      const stock = document.createElement('p');
      stock.style.fontSize = '0.75rem';
      stock.style.color = 'var(--text-secondary)';
      stock.style.marginTop = '4px';
      stock.textContent = `남은 재고: ${item.stock}개`;

      info.appendChild(name);
      info.appendChild(desc);
      info.appendChild(stock);

      const btnBuy = document.createElement('button');
      btnBuy.className = 'btn';
      btnBuy.style.padding = '8px 16px';
      btnBuy.style.fontSize = '0.9rem';
      
      if (item.stock === 0) {
        btnBuy.className = 'btn btn-secondary';
        btnBuy.textContent = '품절';
        btnBuy.disabled = true;
      } else {
        const myData = currentPlayersList.find(p => p.playerId === myPlayerId);
        const hasItem = myData && myData.items && myData.items.length > 0;
        
        if (hasItem) {
          btnBuy.className = 'btn btn-secondary';
          btnBuy.textContent = myData.items.includes(item.itemId) ? '장착됨' : '선택 불가';
          btnBuy.disabled = true;
        } else {
          btnBuy.className = 'btn btn-primary';
          btnBuy.textContent = '구매';
          btnBuy.addEventListener('click', () => {
            btnBuy.disabled = true;
            btnBuy.textContent = '구매 중...';
            socket.emit('buyItem', { playerId: myPlayerId, itemId: item.itemId }, (res) => {
              if (!res.success) {
                alert(`구매 실패: ${res.reason}`);
                btnBuy.disabled = false;
                btnBuy.textContent = '구매';
              } else {
                if (navigator.vibrate) navigator.vibrate(60);
              }
            });
          });
        }
      }

      card.appendChild(info);
      card.appendChild(btnBuy);
      shopItemsContainer.appendChild(card);
    });
  }

  function updateMyItems(playerData) {
    if (!playerData || !playerData.items || playerData.items.length === 0) {
      myItemsListEl.textContent = '구매한 아이템 없음';
      myItemsListEl.style.color = 'var(--text-secondary)';
      myItemsListEl.style.fontWeight = 'normal';
      return;
    }

    myItemsListEl.innerHTML = '';
    myItemsListEl.style.color = 'var(--color-mint-text)';
    myItemsListEl.style.fontWeight = 'bold';
    
    playerData.items.forEach(itemId => {
      const badge = document.createElement('span');
      badge.style.display = 'inline-block';
      badge.style.padding = '3px 8px';
      badge.style.borderRadius = '4px';
      badge.style.fontSize = '0.8rem';
      badge.style.background = 'var(--color-mint-light)';
      badge.style.border = '1px solid var(--color-mint-border)';
      
      let name = itemId;
      if (itemId === 'atk_boost') name = '공격 강화 ⚔️';
      else if (itemId === 'speed_boost') name = '이속 증가 ⚡';
      else if (itemId === 'range_boost') name = '범위 증가 🎯';
      else if (itemId === 'shield') name = '보호막 🛡️';

      badge.textContent = name;
      myItemsListEl.appendChild(badge);
    });
  }

  // Lobby view players renderer
  function updateLobbyPlayers(players) {
    playerChips.innerHTML = '';
    playerCountEl.textContent = `${players.length}명`;

    // Sort players so self is first
    const sorted = [...players].sort((a, b) => {
      if (a.playerId === myPlayerId) return -1;
      if (b.playerId === myPlayerId) return 1;
      return 0;
    });

    sorted.forEach(p => {
      const chip = document.createElement('span');
      chip.className = 'player-chip';
      if (p.playerId === myPlayerId) {
        chip.classList.add('self');
        chip.textContent = `${p.nickname} (나)`;
      } else if (p.isBot) {
        chip.classList.add('bot');
        chip.textContent = p.nickname;
      } else {
        chip.textContent = p.nickname;
      }
      playerChips.appendChild(chip);
    });
  }

  // Active playing view renderer
  function updatePlayingScreen(playerData) {
    if (!playerData) return;
    
    playingNickname.textContent = playerData.nickname;
    
    if (playerData.state === 'Alive') {
      playingStatus.textContent = 'ALIVE';
      playingStatus.className = 'player-status-role status-alive';
      spectatorPanel.style.display = 'none';
      
      // Default HP to 100
      healthValue.textContent = '100';
      healthBar.style.width = '100%';
    } else {
      // Dead or Spectator
      playingStatus.textContent = 'SPECTATOR';
      playingStatus.className = 'player-status-role status-spectator';
      spectatorPanel.style.display = 'block';
      
      // HP to 0
      healthValue.textContent = '0';
      healthBar.style.width = '0%';
      
      // Disable joystick interactivity visually
      joystickTrack.style.opacity = '0.4';
      joystickTrack.style.pointerEvents = 'none';
    }

    // Initialize joystick once when entering screen
    if (!joystickInstance && playerData.state === 'Alive') {
      joystickTrack.style.opacity = '1';
      joystickTrack.style.pointerEvents = 'auto';
      
      // Delay initialization slightly to let the browser paint the active screen
      // and calculate bounds correctly
      setTimeout(() => {
        if (joystickInstance) return; // prevent double initialization
        joystickInstance = new VirtualJoystick(joystickTrack, joystickKnob, {
          onChange: (x, y) => {
            joystickDebug.textContent = `X: ${x.toFixed(2)}, Y: ${eY(y).toFixed(2)}`;
            lastSentVector = { x, y };
          },
          onRelease: () => {
            joystickDebug.textContent = 'X: 0.00, Y: 0.00';
            lastSentVector = { x: 0, y: 0 };
    jumpSeq = 0;
    emoteSeq = 0;
            sendInputToServer(0, 0); // instantly send zero
          }
        });
      }, 50);
    }
  }

  // Helper to invert y values back to negative if we want up as positive on backend.
  // Actually, we negated Y in joystick.js processInput, so up is positive.
  function eY(y) {
    return y;
  }

  function updateMyHealthFromPositionState() {
    const myPosition = playerPositions[myPlayerId];
    if (!myPosition || myPosition.length < 5) return;

    const hp = Number(myPosition[3]);
    const maxHp = Number(myPosition[4]) || 100;
    if (!Number.isFinite(hp) || !Number.isFinite(maxHp) || maxHp <= 0) return;

    const clampedHp = Math.max(0, Math.min(maxHp, hp));
    healthValue.textContent = Math.round(clampedHp).toString();
    healthBar.style.width = `${Math.round((clampedHp / maxHp) * 100)}%`;
  }

  // Results screen renderer
  function showGameResults(data) {
    const isWinner = data.winner && data.winner.playerId === myPlayerId;
    const myRankItemIndex = data.ranking.findIndex(r => r.playerId === myPlayerId);
    
    // Calc rank
    let rankText = '관전자';
    if (isWinner) {
      rankText = '우승! (1등)';
      resultBadge.textContent = '👑';
      resultBadge.className = 'result-badge victory';
      resultTitle.textContent = 'VICTORY';
      resultTitle.className = 'result-title victory';
    } else if (myRankItemIndex !== -1) {
      // ranking has players from first to die to last. So index 0 is first to die.
      // Total players - myIndex is our rank.
      const totalRanked = data.ranking.length;
      const rank = totalRanked - myRankItemIndex + 1; // plus 1 for winner
      rankText = `최종 순위: ${rank}등`;
      resultBadge.textContent = '💀';
      resultBadge.className = 'result-badge';
      resultTitle.textContent = 'DEFEATED';
      resultTitle.className = 'result-title defeated';
    } else {
      resultBadge.textContent = '👁️';
      resultBadge.className = 'result-badge';
      resultTitle.textContent = 'SPECTATED';
      resultTitle.className = 'result-title';
    }

    resultRank.textContent = rankText;
    resultStateTxt.textContent = isWinner ? '생존 (우승)' : '탈락';
    resultTimeTxt.textContent = new Date().toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit' });
  }

  // Helper to get player's Unreal world position (X, Y)
  function getPlayerWorldPos(posData) {
    if (!posData) return { x: 0, y: 0 };
    
    // Index 5 is worldX, Index 6 is worldY
    let wx = posData[5];
    let wy = posData[6];
    
    // If Unreal world coordinates are not available, back up by reversing percentage coordinates
    if (wx === null || wx === undefined || wy === null || wy === undefined) {
      const px = posData[0]; // posX
      const py = posData[1]; // posY
      wy = (px / 100) * 6000 - 3000;
      wx = ((100 - py) / 100) * 6000 - 3000;
    }
    
    return { x: wx, y: wy };
  }

  // HTML5 Canvas Radar / Minimap Renderer
  function drawMinimap() {
    if (!minimapCanvas || !minimapCtx) return;

    // Get client layout bounds to dynamically resize drawing buffer to avoid blurriness/stretching
    const rect = minimapCanvas.getBoundingClientRect();
    const width = rect.width;
    const height = rect.height;
    
    if (minimapCanvas.width !== width || minimapCanvas.height !== height) {
      minimapCanvas.width = width;
      minimapCanvas.height = height;
    }

    // Get "my" position from the positions list
    const myPos = playerPositions[myPlayerId];
    const myWorldPos = getPlayerWorldPos(myPos);

    const cx = width / 2;
    const cy = height / 2;
    // maxRadius is calculated based on the smaller dimension to prevent drawings stretching off-screen vertically
    const maxRadius = Math.min(width, height) / 2 - 10;

    // Clear previous frame
    minimapCtx.clearRect(0, 0, width, height);

    // 1. Draw radar background concentric circles (Grid rings)
    minimapCtx.strokeStyle = '#eef2f6';
    minimapCtx.lineWidth = 1.5;
    
    minimapCtx.beginPath();
    minimapCtx.arc(cx, cy, maxRadius * 0.33, 0, Math.PI * 2);
    minimapCtx.stroke();
    
    minimapCtx.beginPath();
    minimapCtx.arc(cx, cy, maxRadius * 0.66, 0, Math.PI * 2);
    minimapCtx.stroke();

    minimapCtx.beginPath();
    minimapCtx.arc(cx, cy, maxRadius, 0, Math.PI * 2);
    minimapCtx.stroke();

    // Center crosshair lines
    minimapCtx.strokeStyle = 'rgba(203, 213, 225, 0.4)';
    minimapCtx.beginPath();
    minimapCtx.moveTo(cx, 0);
    minimapCtx.lineTo(cx, height);
    minimapCtx.stroke();
    
    minimapCtx.beginPath();
    minimapCtx.moveTo(0, cy);
    minimapCtx.lineTo(width, cy);
    minimapCtx.stroke();

    // 2. Draw Arena Boundary Walls (6000 x 6000 Unreal Units boundary)
    const rx_left = -3000 - myWorldPos.y;
    const ry_top = myWorldPos.x - 3000;
    const arenaLeft = cx + (rx_left / RADAR_VISION_RANGE_UNITS) * maxRadius;
    const arenaTop = cy + (ry_top / RADAR_VISION_RANGE_UNITS) * maxRadius;
    const arenaSize = (6000 / RADAR_VISION_RANGE_UNITS) * maxRadius;

    minimapCtx.strokeStyle = '#ffaaa6'; // Pastel Coral color
    minimapCtx.lineWidth = 4;
    minimapCtx.strokeRect(arenaLeft, arenaTop, arenaSize, arenaSize);

    // Fill outside the arena with a soft pattern or dim it
    minimapCtx.fillStyle = 'rgba(255, 170, 166, 0.03)';
    minimapCtx.fillRect(arenaLeft, arenaTop, arenaSize, arenaSize);

    // 3. Draw other players
    Object.keys(playerPositions).forEach(id => {
      if (id === myPlayerId) return; // Draw me separately at the center
      
      const pos = playerPositions[id];
      const pWorldPos = getPlayerWorldPos(pos);
      const pAlive = pos[2] === 1;

      // Calculate relative vector in Unreal Units
      const rx = pWorldPos.y - myWorldPos.y;
      const ry = -(pWorldPos.x - myWorldPos.x);

      const px = cx + (rx / RADAR_VISION_RANGE_UNITS) * maxRadius;
      const py = cy + (ry / RADAR_VISION_RANGE_UNITS) * maxRadius;

      // Check if within the canvas boundaries (with 15px padding for names)
      if (px >= -15 && px <= width + 15 && py >= -15 && py <= height + 15) {
        // Fetch nickname from cached players list
        const pInfo = currentPlayersList.find(p => p.playerId === id);
        const nickname = pInfo ? pInfo.nickname : '유닛';
        const isBot = pInfo ? pInfo.isBot : id.startsWith('bot_');

        // Draw dot
        minimapCtx.beginPath();
        minimapCtx.arc(px, py, 7.5, 0, Math.PI * 2); // Increased dot size slightly
        
        if (!pAlive) {
          minimapCtx.fillStyle = '#cbd5e1'; // Dead: gray
        } else if (isBot) {
          minimapCtx.fillStyle = '#dcedc1'; // Bot: soft lavender
        } else {
          minimapCtx.fillStyle = '#a8d8ea'; // Player: soft blue
        }
        
        minimapCtx.fill();
        minimapCtx.strokeStyle = '#ffffff';
        minimapCtx.lineWidth = 1.5;
        minimapCtx.stroke();

        // Draw player nickname text (1.5x larger, Slate-800, with white outline for max legibility)
        minimapCtx.font = 'bold 13px Inter, sans-serif';
        minimapCtx.textAlign = 'center';
        
        minimapCtx.strokeStyle = '#ffffff';
        minimapCtx.lineWidth = 3.5;
        minimapCtx.strokeText(nickname, px, py - 11);

        minimapCtx.fillStyle = '#1e293b';
        minimapCtx.fillText(nickname, px, py - 11);
      }
    });

    // 4. Draw "me" at the center
    // Radar pulse wave
    minimapCtx.strokeStyle = 'rgba(168, 230, 207, 0.4)';
    minimapCtx.lineWidth = 2.5;
    minimapCtx.beginPath();
    const pulseRadius = 14 + (Date.now() / 25) % 45;
    minimapCtx.arc(cx, cy, pulseRadius, 0, Math.PI * 2);
    minimapCtx.stroke();

    // Me center dot
    minimapCtx.beginPath();
    minimapCtx.arc(cx, cy, 11, 0, Math.PI * 2);
    minimapCtx.fillStyle = '#a8e6cf'; // Soft Mint
    minimapCtx.fill();
    minimapCtx.strokeStyle = '#ffffff';
    minimapCtx.lineWidth = 3;
    minimapCtx.stroke();
  }

  // --- 4. Inputs & Actions Handler ---

  // Handle Nickname Submission
  btnJoin.addEventListener('click', () => {
    const nickname = nicknameInput.value.trim();
    if (nickname === '') {
      alert('닉네임을 입력해 주세요!');
      return;
    }

    btnJoin.disabled = true;
    btnJoin.textContent = '입장 중...';

    socket.emit('join', { nickname }, (response) => {
      btnJoin.disabled = false;
      btnJoin.textContent = '입장하기';

      if (response.success) {
        myPlayerId = response.playerId;
        myNickname = response.nickname;
        myState = response.state;

        sessionStorage.setItem('showdown_playerId', myPlayerId);
        sessionStorage.setItem('showdown_nickname', myNickname);

        if (currentGameState === 'Playing') {
          showScreen(screenPlaying);
          socket.emit('rejoin', { playerId: myPlayerId }, (res) => {
            if (res.success) {
              myState = res.state;
              updatePlayingScreen(res);
              startInputSending();
            }
          });
        } else {
          showScreen(screenLobby);
        }
      } else {
        alert(`입장 실패: ${response.reason}`);
      }
    });
  });

  // Re-enter lobby after result
  btnLobbyReturn.addEventListener('click', () => {
    socket.emit('rejoin', { playerId: myPlayerId }, (res) => {
      if (res.success) {
        myState = res.state;
        showScreen(screenLobby);
      } else {
        clearSession();
        showScreen(screenJoin);
      }
    });
  });

  // Spectator Voting buttons
  const voteButtons = document.querySelectorAll('.btn-vote');
  voteButtons.forEach(btn => {
    btn.addEventListener('click', (e) => {
      const eventType = btn.getAttribute('data-event');
      // Trigger vibration
      if (navigator.vibrate) {
        navigator.vibrate(50);
      }
      
      // Disable buttons temporarily to prevent spamming
      voteButtons.forEach(b => b.disabled = true);
      btn.style.backgroundColor = 'var(--color-mint)';
      btn.style.color = 'var(--color-mint-text)';
      
      // Send vote to server
      socket.emit('voteEvent', { playerId: myPlayerId, eventType });
      
      setTimeout(() => {
        voteButtons.forEach(b => {
          b.disabled = false;
          b.style.backgroundColor = '';
          b.style.color = '';
        });
      }, 3000);
    });
  });

  if (btnJump) {
    btnJump.addEventListener('click', () => {
      if (myState !== 'Alive' || currentGameState !== 'Playing') return;
      jumpSeq += 1;
      sendInputToServer(lastSentVector.x, lastSentVector.y);
    });
  }

  if (btnEmote) {
    btnEmote.addEventListener('click', () => {
      if (myState !== 'Alive' || currentGameState !== 'Playing') return;
      emoteSeq += 1;
      sendInputToServer(lastSentVector.x, lastSentVector.y);
    });
  }

  // Input sending timer (100ms interval)
  function startInputSending() {
    if (inputInterval) clearInterval(inputInterval);
    
    inputInterval = setInterval(() => {
      if (myState === 'Alive' && currentGameState === 'Playing') {
        // Send inputs
        sendInputToServer(lastSentVector.x, lastSentVector.y);
      }
    }, 100);
  }

  function stopInputSending() {
    if (inputInterval) {
      clearInterval(inputInterval);
      inputInterval = null;
    }
    joystickInstance = null; // reset instance so it can recreate next game
    lastSentVector = { x: 0, y: 0 };
    jumpSeq = 0;
    emoteSeq = 0;
  }

  function sendInputToServer(x, y) {
    socket.emit('moveInput', { moveX: x, moveY: y, jumpSeq, emoteSeq });
  }
});


