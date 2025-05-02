const ws = new WebSocket("ws://localhost:8080");
ws.onmessage = (e) => console.log("Ответ сервера:", e.data);
ws.onopen = () => ws.send("Привет, сервер!");