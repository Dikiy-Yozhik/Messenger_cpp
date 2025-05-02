document.addEventListener('DOMContentLoaded', function() {
    // Отправка сообщения
    const messageInput = document.querySelector('.message-input input');
    const sendButton = document.querySelector('.send-button');
    const messagesContainer = document.querySelector('.messages');
    
    function sendMessage() {
        const text = messageInput.value.trim();
        if (text === '') return;
        
        const messageElement = document.createElement('div');
        messageElement.className = 'message outgoing';
        messageElement.innerHTML = `
            <div class="message-text">${text}</div>
            <div class="message-time">${getCurrentTime()}</div>
        `;
        
        messagesContainer.appendChild(messageElement);
        messageInput.value = '';
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }
    
    sendButton.addEventListener('click', sendMessage);
    messageInput.addEventListener('keypress', function(e) {
        if (e.key === 'Enter') sendMessage();
    });
    
    // Выбор чата
    const chatItems = document.querySelectorAll('.chat-item');
    chatItems.forEach(item => {
        item.addEventListener('click', function() {
            chatItems.forEach(i => i.classList.remove('active'));
            this.classList.add('active');
            
            // Здесь можно загрузить сообщения выбранного чата
            const contactName = this.querySelector('.chat-name').textContent;
            document.querySelector('.contact-name').textContent = contactName;
        });
    });
    
    function getCurrentTime() {
        const now = new Date();
        return `${now.getHours()}:${now.getMinutes().toString().padStart(2, '0')}`;
    }
});