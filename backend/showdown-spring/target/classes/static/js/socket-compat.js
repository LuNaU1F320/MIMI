(function () {
  function createSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const handlers = {};
    const pendingCallbacks = {};
    const ACK_TIMEOUT_MS = 8000;
    let requestCounter = 0;
    let connected = false;
    let socket = null;

    function trigger(event, payload) {
      (handlers[event] || []).forEach(handler => handler(payload));
    }

    function connect() {
      socket = new WebSocket(wsUrl);

      socket.addEventListener('open', () => {
        connected = true;
        trigger('connect');
      });

      socket.addEventListener('close', () => {
        connected = false;
        Object.keys(pendingCallbacks).forEach(requestId => {
          pendingCallbacks[requestId].callback({ success: false, reason: '서버 연결이 끊겼습니다.' });
          clearTimeout(pendingCallbacks[requestId].timer);
          delete pendingCallbacks[requestId];
        });
        trigger('disconnect');
        setTimeout(connect, 1000);
      });

      socket.addEventListener('message', event => {
        let message;
        try {
          message = JSON.parse(event.data);
        } catch (err) {
          console.error('Invalid websocket message:', event.data);
          return;
        }

        if (message.requestId && pendingCallbacks[message.requestId]) {
          const pending = pendingCallbacks[message.requestId];
          clearTimeout(pending.timer);
          delete pendingCallbacks[message.requestId];
          pending.callback(message.payload);
          return;
        }

        trigger(message.event, message.payload);
      });
    }

    connect();

    return {
      on(event, handler) {
        handlers[event] = handlers[event] || [];
        handlers[event].push(handler);
      },
      emit(event, payload, callback) {
        const requestId = callback ? `req_${++requestCounter}` : undefined;
        if (callback) {
          const timer = setTimeout(() => {
            if (!pendingCallbacks[requestId]) return;
            delete pendingCallbacks[requestId];
            callback({ success: false, reason: '서버 응답 시간이 초과되었습니다.' });
          }, ACK_TIMEOUT_MS);
          pendingCallbacks[requestId] = { callback, timer };
        }

        const sendMessage = () => {
          socket.send(JSON.stringify({ event, payload: payload || {}, requestId }));
        };

        if (connected && socket.readyState === WebSocket.OPEN) {
          sendMessage();
        } else {
          const timer = setInterval(() => {
            if (connected && socket.readyState === WebSocket.OPEN) {
              clearInterval(timer);
              sendMessage();
            }
          }, 50);
        }
      }
    };
  }

  window.io = createSocket;
})();
