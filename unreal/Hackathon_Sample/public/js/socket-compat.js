(function () {
  function createSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const handlers = {};
    const pendingCallbacks = {};
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
          pendingCallbacks[message.requestId](message.payload);
          delete pendingCallbacks[message.requestId];
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
        if (callback) pendingCallbacks[requestId] = callback;

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
